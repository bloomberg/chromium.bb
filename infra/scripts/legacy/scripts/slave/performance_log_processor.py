# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains PerformanceLogProcessor and subclasses.

Several performance tests have complicated log output, this module is intended
to help buildsteps parse these logs and identify if tests had anomalies.

The classes in this file all have the same method ProcessLine, just like
GTestLogParser in //tools/build/scripts/common/gtest_utils.py. They also
construct a set of files which are used for graphing.

Note: This module is doomed to be deprecated in the future, as Telemetry
results will be passed more directly to the new performance dashboard.
"""

import collections
import json
import logging
import os
import re

from common import chromium_utils

# TODO(crbug.com/403564).
import config

# Status codes that can be returned by the evaluateCommand method.
# From buildbot.status.builder.
# See: http://docs.buildbot.net/current/developer/results.html
SUCCESS, WARNINGS, FAILURE, SKIPPED, EXCEPTION, RETRY = range(6)


class PerformanceLogProcessor(object):
  """Parent class for performance log parsers.

  The only essential public method that subclasses must define is the method
  ProcessLine, which takes one line of a test output log and uses it
  to change the internal state of the PerformanceLogProcessor object,
  so that methods such as PerformanceLogs return the right thing.
  """

  # The file perf_expectations.json holds performance expectations.
  # For more info, see: http://goo.gl/BhYvDa
  PERF_EXPECTATIONS_PATH = 'src/tools/perf_expectations/'

  def __init__(self, revision=None, factory_properties=None,
               build_properties=None, webkit_revision='undefined'):
    """Initializes the log processor.

    Args:
      revision: Revision number; this currently could also be a git number.
          It is sent to the perf dashboard to be used as the x-value.
      factory_properties: Factory properties dict.
      build_properties: Build properties dict.
      webkit_revision: Blink revision number.
    """
    if factory_properties is None:
      factory_properties = {}

    # Performance regression/speedup alerts.
    self._read_expectations = False

    self._perf_id = factory_properties.get('perf_id')
    self._perf_name = factory_properties.get('perf_name')
    self._perf_filename = factory_properties.get('perf_filename')
    self._test_name = factory_properties.get('test_name')

    self._perf_data = {}
    self._perf_test_keys = {}
    self._perf_ref_keys = {}
    self._perf_regress = []
    self._perf_improve = []

    # A dict mapping output file names to lists of lines in a file.
    self._output = {}

    # Whether or not the processing has been finalized (i.e. whether
    # self._FinalizeProcessing has been called.)
    self._finalized = False

    # The text summary will be built by other methods as we go.
    # This is a list of strings with messages about the processing.
    self._text_summary = []

    # Enable expectations if the local configuration supports it.
    self._expectations = (factory_properties.get('expectations')
                          and self._perf_id and self._perf_name)
    if self._expectations and not self._perf_filename:
      self._perf_filename = os.path.join(self.PERF_EXPECTATIONS_PATH,
                                         'perf_expectations.json')

    if revision:
      self._revision = revision
    else:
      raise ValueError('Must provide a revision to PerformanceLogProcessor.')

    self._webkit_revision = webkit_revision

    if not build_properties:
      build_properties = {}
    self._git_revision = build_properties.get('git_revision', 'undefined')
    self._version = build_properties.get('version', 'undefined')
    self._channel = build_properties.get('channel', 'undefined')
    self._webrtc_revision = build_properties.get('got_webrtc_revision',
                                                 'undefined')

    self._v8_revision = 'undefined'
    if factory_properties.get('show_v8_revision'):
      self._v8_revision = build_properties.get('got_v8_revision', 'undefined')

    self._percentiles = [.1, .25, .5, .75, .90, .95, .99]

  def IsChartJson(self):  # pylint: disable=R0201
    """This is not the new telemetry --chartjson output format."""
    return False

  def PerformanceLogs(self):
    if not self._finalized:
      self._FinalizeProcessing()
      self._finalized = True
    return self._output

  def PerformanceSummary(self):
    """Returns a list of strings about performance changes and other info."""
    if not self._finalized:
      self._FinalizeProcessing()
      self._finalized = True
    return self.PerformanceChanges() + self._text_summary

  def _FinalizeProcessing(self):
    """Hook for subclasses to do final operations before output is returned."""
    # This method is to be defined by inheriting classes.
    pass

  def AppendLog(self, filename, data):
    """Appends some data to an output file."""
    self._output[filename] = self._output.get(filename, []) + data

  def PrependLog(self, filename, data):
    """Prepends some data to an output file."""
    self._output[filename] = data + self._output.get(filename, [])

  def FailedTests(self):  # pylint: disable=R0201
    return []

  def MemoryToolReportHashes(self):  # pylint: disable=R0201
    return []

  def ParsingErrors(self):  # pylint: disable=R0201
    return []

  def LoadPerformanceExpectationsData(self, all_perf_data):
    """Load the expectations data.

    All keys in perf_expectations have 4 components:
      slave/test/graph/trace

    LoadPerformanceExpectationsData finds all keys that match the initial
    portion of the string ("slave/test") and adds the graph and result
    portions to the expected performance structure.
    """

    for perf_key in all_perf_data.keys():
      # tools/perf_expectations/tests/perf_expectations_unittest.py should have
      # a matching regular expression.
      m = re.search(r'^' + self._perf_name + '/' + self._test_name +
                    r'/([\w\.-]+)/([\w\.-]+)$', perf_key)
      if not m:
        continue

      perf_data = all_perf_data[perf_key]
      graph = m.group(1)
      trace = m.group(2)

      # By default, all perf data is type=relative.
      perf_data.setdefault('type', 'relative')

      # By default, relative perf data is compare against the fqtn+'_ref'.
      if perf_data['type'] == 'relative' and 'ref' not in perf_data:
        perf_data['ref'] = '%s/%s/%s/%s_ref' % (
            self._perf_name, self._test_name, graph, trace)

      # For each test key, we add a reference in _perf_test_keys to perf_data.
      self._perf_test_keys.setdefault(perf_key, [])
      self._perf_test_keys[perf_key].append(perf_data)

      # For each ref key, we add a reference in _perf_ref_keys to perf_data.
      if 'ref' in perf_data:
        self._perf_ref_keys.setdefault(perf_data['ref'], [])
        self._perf_ref_keys[perf_data['ref']].append(perf_data)

      self._perf_data.setdefault(graph, {})
      self._perf_data[graph][trace] = perf_data

  def LoadPerformanceExpectations(self):
    if not self._expectations:
      # self._expectations is false when a given factory doesn't enable
      # expectations, or doesn't have both perf_id and perf_name set.
      return
    try:
      perf_file = open(self._perf_filename, 'r')
    except IOError, e:
      logging.error('I/O Error reading expectations %s(%s): %s' %
                    (self._perf_filename, e.errno, e.strerror))
      return

    perf_data = {}
    if perf_file:
      try:
        perf_data = json.load(perf_file)
      except ValueError:
        perf_file.seek(0)
        logging.error('Error parsing expectations %s: \'%s\'' %
                      (self._perf_filename, perf_file.read().strip()))
      perf_file.close()

    # Find this perf/test entry
    if perf_data and perf_data.has_key('load') and perf_data['load']:
      self.LoadPerformanceExpectationsData(perf_data)
    else:
      logging.error('not loading perf expectations: perf_data is disabled')
    self._read_expectations = True

  def TrackActualPerformance(self, graph=None, trace=None, value=None,
                             stddev=None):
    """Set actual performance data when we come across useful values.

    trace will be of the form "RESULTTYPE" or "RESULTTYPE_ref".
    A trace with _ref in its name refers to a reference build.

    Common result types for page cyclers: t, vm_rss_f_r, IO_b_b, etc.
    A test's result types vary between test types.  Currently, a test
    only needs to output the appropriate text format to embed a new
    result type.
    """

    fqtn = '%s/%s/%s/%s' % (self._perf_name, self._test_name, graph, trace)
    if fqtn in self._perf_test_keys:
      for perf_data in self._perf_test_keys[fqtn]:
        perf_data['actual_test'] = value
        perf_data['actual_var'] = stddev

        if perf_data['type'] == 'absolute' and 'actual_test' in perf_data:
          perf_data['actual_delta'] = perf_data['actual_test']

        elif perf_data['type'] == 'relative':
          if 'actual_test' in perf_data and 'actual_ref' in perf_data:
            perf_data['actual_delta'] = (
                perf_data['actual_test'] - perf_data['actual_ref'])

    if fqtn in self._perf_ref_keys:
      for perf_data in self._perf_ref_keys[fqtn]:
        perf_data['actual_ref'] = value

        if 'actual_test' in perf_data and 'actual_ref' in perf_data:
          perf_data['actual_delta'] = (
              perf_data['actual_test'] - perf_data['actual_ref'])

  def PerformanceChangesAsText(self):
    """Returns a list of strings which describe performance changes."""

    text = []

    if self._expectations and not self._read_expectations:
      text.append('MISS_EXPECTATIONS')

    if self._perf_regress:
      text.append('PERF_REGRESS: ' + ', '.join(self._perf_regress))

    if self._perf_improve:
      text.append('PERF_IMPROVE: ' + ', '.join(self._perf_improve))

    return text

  def ComparePerformance(self, graph, trace):
    """Populates internal data about improvements and regressions."""
    # Skip graphs and traces we don't expect values for.
    if not graph in self._perf_data or not trace in self._perf_data[graph]:
      return

    perfdata = self._perf_data[graph][trace]
    graph_result = graph + '/' + trace

    # Skip result types that didn't calculate a delta.
    if not 'actual_delta' in perfdata:
      return

    # Skip result types that don't have regress/improve values.
    if 'regress' not in perfdata or 'improve' not in perfdata:
      return

    # Set the high and low performance tests.
    # The actual delta needs to be within this range to keep the perf test
    # green.  If the results fall above or below this range, the test will go
    # red (signaling a regression) or orange (signaling a speedup).
    actual = perfdata['actual_delta']
    regress = perfdata['regress']
    improve = perfdata['improve']
    if (('better' in perfdata and perfdata['better'] == 'lower') or
        ('better' not in perfdata and regress > improve)):
      # The "lower is better" case.  (ie. time results)
      if actual < improve:
        ratio = 1 - _Divide(actual, improve)
        self._perf_improve.append('%s (%s)' % (graph_result,
                                               _FormatPercentage(ratio)))
      elif actual > regress:
        ratio = _Divide(actual, regress) - 1
        self._perf_regress.append('%s (%s)' % (graph_result,
                                               _FormatPercentage(ratio)))
    else:
      # The "higher is better" case.  (ie. score results)
      if actual > improve:
        ratio = _Divide(actual, improve) - 1
        self._perf_improve.append('%s (%s)' % (graph_result,
                                               _FormatPercentage(ratio)))
      elif actual < regress:
        ratio = 1 - _Divide(actual, regress)
        self._perf_regress.append('%s (%s)' % (graph_result,
                                               _FormatPercentage(ratio)))

  def PerformanceChanges(self):
    """Compares actual and expected results.

    Returns:
      A list of strings indicating improvements or regressions.
    """
    # Compare actual and expected results.
    for graph in self._perf_data:
      for trace in self._perf_data[graph]:
        self.ComparePerformance(graph, trace)

    return self.PerformanceChangesAsText()

  # Unused argument cmd.
  # pylint: disable=W0613
  def evaluateCommand(self, cmd):
    """Returns a status code indicating success, failure, etc.

    See: http://docs.buildbot.net/current/developer/cls-buildsteps.html

    Args:
      cmd: A command object. Not used here.

    Returns:
      A status code (One of SUCCESS, WARNINGS, FAILURE, etc.)
    """
    if self._expectations and not self._read_expectations:
      return WARNINGS

    # make sure regression and improvement logs are calculated
    self.PerformanceSummary()

    if self._perf_regress:
      return FAILURE

    if self._perf_improve:
      return WARNINGS

    # There was no change in performance, report success.
    return SUCCESS

  def ProcessLine(self, line):
    """Process one line of a log file."""
    # This method must be overridden by subclass
    pass


class GraphingLogProcessor(PerformanceLogProcessor):
  """Parent class for any log processor expecting standard data to be graphed.

  The log will be parsed looking for any lines of the forms:
    <*>RESULT <graph_name>: <trace_name>= <value> <units>
  or
    <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...] <units>
  or
    <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>

  For example,
    *RESULT vm_final_browser: OneTab= 8488 kb
    RESULT startup: ref= [167.00,148.00,146.00,142.00] ms
    RESULT TabCapturePerformance_foo: Capture= {30.7, 1.45} ms

  The leading * is optional; it indicates that the data from that line should
  be considered "important", which may mean for example that it's graphed by
  default.

  If multiple values are given in [], their mean and (sample) standard
  deviation will be written; if only one value is given, that will be written.
  A trailing comma is permitted in the list of values.

  NOTE: All lines except for RESULT lines are ignored, including the Avg and
  Stddev lines output by Telemetry!

  Any of the <fields> except <value> may be empty, in which case the
  not-terribly-useful defaults will be used. The <graph_name> and <trace_name>
  should not contain any spaces, colons (:) nor equals-signs (=). Furthermore,
  the <trace_name> will be used on the waterfall display, so it should be kept
  short. If the trace_name ends with '_ref', it will be interpreted as a
  reference value, and shown alongside the corresponding main value on the
  waterfall.

  Semantic note: The terms graph and chart are used interchangeably here.
  """

  # The file into which the GraphingLogProcessor will save a list of graph
  # names for use by the JS doing the plotting.
  GRAPH_LIST = config.Master.perf_graph_list

  RESULTS_REGEX = re.compile(r'(?P<IMPORTANT>\*)?RESULT '
                             r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                             r'(?P<VALUE>[\{\[]?[-\d\., ]+[\}\]]?)('
                             r' ?(?P<UNITS>.+))?')
  HISTOGRAM_REGEX = re.compile(r'(?P<IMPORTANT>\*)?HISTOGRAM '
                               r'(?P<GRAPH>[^:]*): (?P<TRACE>[^=]*)= '
                               r'(?P<VALUE_JSON>{.*})(?P<UNITS>.+)?')

  class Trace(object):
    """Encapsulates data for one trace. Here, this means one point."""

    def __init__(self):
      self.important = False
      self.value = 0.0
      self.stddev = 0.0

    def __str__(self):
      result = _FormatHumanReadable(self.value)
      if self.stddev:
        result += '+/-%s' % _FormatHumanReadable(self.stddev)
      return result

  class Graph(object):
    """Encapsulates a set of points that should appear on the same graph."""

    def __init__(self):
      self.units = None
      self.traces = {}

    def IsImportant(self):
      """A graph is considered important if any of its traces is important."""
      for trace in self.traces.itervalues():
        if trace.important:
          return True
      return False

    def BuildTracesDict(self):
      """Returns a dictionary mapping trace names to [value, stddev]."""
      traces_dict = {}
      for name, trace in self.traces.items():
        traces_dict[name] = [str(trace.value), str(trace.stddev)]
      return traces_dict

  def __init__(self, *args, **kwargs):
    """Initiates this log processor."""
    PerformanceLogProcessor.__init__(self, *args, **kwargs)

    # A dict of Graph objects, by name.
    self._graphs = {}

    # Load performance expectations for this test.
    self.LoadPerformanceExpectations()

  def ProcessLine(self, line):
    """Processes one result line, and updates the state accordingly."""
    results_match = self.RESULTS_REGEX.search(line)
    histogram_match = self.HISTOGRAM_REGEX.search(line)
    if results_match:
      self._ProcessResultLine(results_match)
    elif histogram_match:
      self._ProcessHistogramLine(histogram_match)

  def _ProcessResultLine(self, line_match):
    """Processes a line that matches the standard RESULT line format.

    Args:
      line_match: A MatchObject as returned by re.search.
    """
    match_dict = line_match.groupdict()
    graph_name = match_dict['GRAPH'].strip()
    trace_name = match_dict['TRACE'].strip()

    graph = self._graphs.get(graph_name, self.Graph())
    graph.units = match_dict['UNITS'] or ''
    trace = graph.traces.get(trace_name, self.Trace())
    trace.value = match_dict['VALUE']
    trace.important = match_dict['IMPORTANT'] or False

    # Compute the mean and standard deviation for a multiple-valued item,
    # or the numerical value of a single-valued item.
    if trace.value.startswith('['):
      try:
        value_list = [float(x) for x in trace.value.strip('[],').split(',')]
      except ValueError:
        # Report, but ignore, corrupted data lines. (Lines that are so badly
        # broken that they don't even match the RESULTS_REGEX won't be
        # detected.)
        logging.warning("Bad test output: '%s'" % trace.value.strip())
        return
      trace.value, trace.stddev, filedata = self._CalculateStatistics(
          value_list, trace_name)
      assert filedata is not None
      for filename in filedata:
        self.PrependLog(filename, filedata[filename])
    elif trace.value.startswith('{'):
      stripped = trace.value.strip('{},')
      try:
        trace.value, trace.stddev = [float(x) for x in stripped.split(',')]
      except ValueError:
        logging.warning("Bad test output: '%s'" % trace.value.strip())
        return
    else:
      try:
        trace.value = float(trace.value)
      except ValueError:
        logging.warning("Bad test output: '%s'" % trace.value.strip())
        return

    graph.traces[trace_name] = trace
    self._graphs[graph_name] = graph

    # Store values in actual performance.
    self.TrackActualPerformance(graph=graph_name, trace=trace_name,
                                value=trace.value, stddev=trace.stddev)

  def _ProcessHistogramLine(self, line_match):
    """Processes a line that matches the HISTOGRAM line format.

    Args:
      line_match: A MatchObject as returned by re.search.
    """
    match_dict = line_match.groupdict()
    graph_name = match_dict['GRAPH'].strip()
    trace_name = match_dict['TRACE'].strip()
    units = (match_dict['UNITS'] or '').strip()
    histogram_json = match_dict['VALUE_JSON']
    important = match_dict['IMPORTANT'] or False
    try:
      histogram_data = json.loads(histogram_json)
    except ValueError:
      # Report, but ignore, corrupted data lines. (Lines that are so badly
      # broken that they don't even match the HISTOGRAM_REGEX won't be
      # detected.)
      logging.warning("Bad test output: '%s'" % histogram_json.strip())
      return

    # Compute percentile data, create a graph for all percentile values.
    percentiles = self._CalculatePercentiles(histogram_data, trace_name)
    for i in percentiles:
      percentile_graph_name = graph_name + "_" + str(i['percentile'])
      graph = self._graphs.get(percentile_graph_name, self.Graph())
      graph.units = units
      trace = graph.traces.get(trace_name, self.Trace())
      trace.value = i['value']
      trace.important = important
      graph.traces[trace_name] = trace
      self._graphs[percentile_graph_name] = graph
      self.TrackActualPerformance(graph=percentile_graph_name,
                                  trace=trace_name,
                                  value=i['value'])

    # Compute geometric mean and standard deviation.
    graph = self._graphs.get(graph_name, self.Graph())
    graph.units = units
    trace = graph.traces.get(trace_name, self.Trace())
    trace.value, trace.stddev = self._CalculateHistogramStatistics(
        histogram_data, trace_name)
    trace.important = important
    graph.traces[trace_name] = trace
    self._graphs[graph_name] = graph
    self.TrackActualPerformance(graph=graph_name, trace=trace_name,
                                value=trace.value, stddev=trace.stddev)

  # _CalculateStatistics needs to be a member function.
  # pylint: disable=R0201
  # Unused argument value_list.
  # pylint: disable=W0613
  def _CalculateStatistics(self, value_list, trace_name):
    """Returns a tuple with some statistics based on the given value list.

    This method may be overridden by subclasses wanting a different standard
    deviation calcuation (or some other sort of error value entirely).

    Args:
      value_list: the list of values to use in the calculation
      trace_name: the trace that produced the data (not used in the base
          implementation, but subclasses may use it)

    Returns:
      A 3-tuple - mean, standard deviation, and a dict which is either
          empty or contains information about some file contents.
    """
    mean, stddev = chromium_utils.MeanAndStandardDeviation(value_list)
    return mean, stddev, {}

  def _CalculatePercentiles(self, histogram, trace_name):
    """Returns a list of percentile values from a histogram.

    This method may be overridden by subclasses.

    Args:
      histogram: histogram data (relevant keys: "buckets", and for each bucket,
          "min", "max" and "count").
      trace_name: the trace that produced the data (not used in the base
          implementation, but subclasses may use it)

    Returns:
      A list of dicts, each of which has the keys "percentile" and "value".
    """
    return chromium_utils.HistogramPercentiles(histogram, self._percentiles)

  def _CalculateHistogramStatistics(self, histogram, trace_name):
    """Returns the geometric mean and standard deviation for a histogram.

    This method may be overridden by subclasses.

    Args:
      histogram: histogram data (relevant keys: "buckets", and for each bucket,
          "min", "max" and "count").
      trace_name: the trace that produced the data (not used in the base
          implementation, but subclasses may use it)
    """
    geom_mean, stddev = chromium_utils.GeomMeanAndStdDevFromHistogram(histogram)
    return geom_mean, stddev

  def _BuildSummaryJson(self, graph):
    """Returns JSON with the data in the given graph plus revision information.

    Args:
      graph: A GraphingLogProcessor.Graph object.

    Returns:
      The format output here is the "-summary.dat line" format; that is, it
      is a JSON encoding of a dictionary that has the key "traces"
    """
    assert self._revision, 'revision must always be present'

    graph_dict = collections.OrderedDict([
        ('traces', graph.BuildTracesDict()),
        ('rev', str(self._revision)),
        ('git_revision', str(self._git_revision)),
        ('webkit_rev', str(self._webkit_revision)),
        ('webrtc_rev', str(self._webrtc_revision)),
        ('v8_rev', str(self._v8_revision)),
        ('ver', str(self._version)),
        ('chan', str(self._channel)),
        ('units', str(graph.units)),
    ])

    # Include a sorted list of important trace names if there are any.
    important = [t for t in graph.traces.keys() if graph.traces[t].important]
    if important:
      graph_dict['important'] = sorted(important)

    return json.dumps(graph_dict)

  def _FinalizeProcessing(self):
    self._CreateSummaryOutput()
    self._GenerateGraphInfo()

  def _CreateSummaryOutput(self):
    """Writes the summary data file and collect the waterfall display text.

    The summary file contains JSON-encoded data.

    The waterfall contains lines for each important trace, in the form
      tracename: value< (refvalue)>
    """

    for graph_name, graph in self._graphs.iteritems():
      # Write a line in the applicable summary file for each graph.
      filename = ('%s-summary.dat' % graph_name)
      data = [self._BuildSummaryJson(graph) + '\n']
      self._output[filename] = data + self._output.get(filename, [])

      # Add a line to the waterfall for each important trace.
      for trace_name, trace in graph.traces.iteritems():
        if trace_name.endswith('_ref'):
          continue
        if trace.important:
          display = '%s: %s' % (trace_name, _FormatHumanReadable(trace.value))
          if graph.traces.get(trace_name + '_ref'):
            display += ' (%s)' % _FormatHumanReadable(
                graph.traces[trace_name + '_ref'].value)
          self._text_summary.append(display)

    self._text_summary.sort()

  def _GenerateGraphInfo(self):
    """Outputs a list of graphs viewed this session, for use by the plotter.

    These will be collated and sorted on the master side.
    """
    graphs = {}
    for name, graph in self._graphs.iteritems():
      graphs[name] = {'name': name,
                      'important': graph.IsImportant(),
                      'units': graph.units}
    self._output[self.GRAPH_LIST] = json.dumps(graphs).split('\n')

  def GetGraphs(self):
    """Returns a list of graph names."""
    return self._graphs.keys()

  def GetTraces(self, graph):
    """Returns a dict of traces associated with the given graph.

    Returns:
      A dict mapping names of traces to two-element lists of value, stddev.
    """
    return self._graphs[graph].BuildTracesDict()

  def GetUnits(self, graph):
    """Returns the units associated with the given graph."""
    return str(self._graphs[graph].units)


class GraphingPageCyclerLogProcessor(GraphingLogProcessor):
  """Handles additional processing for page-cycler timing data."""

  _page_list = ['(unknown)']
  PAGES_REGEXP = re.compile(r'^Pages: \[(?P<LIST>.*)\]')

  def ProcessLine(self, line):
    """Also looks for the Pages: line to find the page count."""
    # super() should be used instead of GetParentClass().
    # pylint: disable=W0212
    line_match = self.PAGES_REGEXP.search(line)
    if line_match:
      self._page_list = line_match.groupdict()['LIST'].strip().split(',')
      if len(self._page_list) < 1:
        self._page_list = ['(unknown)']
    else:
      chromium_utils.GetParentClass(self).ProcessLine(self, line)

  def _CalculateStatistics(self, value_list, trace_name):
    """Handles statistics generation and recording for page-cycler data.

    Sums the timings over all pages for each iteration and returns a tuple
    (mean, standard deviation) of those sums.  Also saves a data file
    <revision>_<tracename>.dat holding a line of times for each URL loaded,
    for use by humans when debugging a regression.
    """

    # If the name of the trace is one of the pages in the page list then we are
    # dealing with the results for that page only, not the overall results. So
    # calculate the statistics like a normal GraphingLogProcessor, not the
    # GraphingPageCyclerLogProcessor.
    if trace_name in self._page_list:
      return super(GraphingPageCyclerLogProcessor, self)._CalculateStatistics(
          value_list, trace_name)

    value_count = len(value_list)
    page_count = len(self._page_list)
    # Chunk value_list into groups, where each sub-list
    # has all the page times of one iteration.
    iterations = [value_list[start:start+page_count]
                  for start in xrange(0, value_count, page_count)]

    iteration_times = map(sum, iterations)
    page_times_list = map(list, zip(*iterations))
    page_times_dict = dict(zip(self._page_list, page_times_list))

    pagedata = self._SavePageData(page_times_dict, trace_name)
    val, stddev = chromium_utils.FilteredMeanAndStandardDeviation(
        iteration_times)
    return val, stddev, pagedata

  def _SavePageData(self, page_times, trace_name):
    """Saves a file holding the timing data for each page loaded.

    Args:
      page_times: a dict mapping a page URL to a list of its times
      trace_name: the trace that produced this set of times

    Returns:
      A dict with one entry, mapping filename to file contents.
    """
    file_data = []
    for page, times in sorted(page_times.iteritems()):
      mean, stddev = chromium_utils.FilteredMeanAndStandardDeviation(times)
      file_data.append('%s (%s+/-%s): %s' % (page,
                                             _FormatFloat(mean),
                                             _FormatFloat(stddev),
                                             _JoinWithSpacesAndNewLine(times)))

    filename = '%s_%s.dat' % (self._revision, trace_name)
    return {filename: file_data}


def _FormatFloat(number):
  """Formats float with two decimal points."""
  if number:
    return '%.2f' % number
  else:
    return '0.00'


def _FormatPercentage(ratio):
  """Formats a number as a string with a percentage (e.g. 0.5 => "50%")."""
  return '%s%%' % _FormatFloat(100 * ratio)


def _Divide(x, y):
  """Divides with float division, or returns infinity if denominator is 0."""
  if y == 0:
    return float('inf')
  return float(x) / y


def _FormatHumanReadable(number):
  """Formats a float into three significant figures, using metric suffixes.

  Only m, k, and M prefixes (for 1/1000, 1000, and 1,000,000) are used.
  Examples:
    0.0387    => 38.7m
    1.1234    => 1.12
    10866     => 10.8k
    682851200 => 683M
  """
  metric_prefixes = {-3: 'm', 0: '', 3: 'k', 6: 'M'}
  scientific = '%.2e' % float(number)     # 6.83e+005
  e_idx = scientific.find('e')            # 4, or 5 if negative
  digits = float(scientific[:e_idx])      # 6.83
  exponent = int(scientific[e_idx + 1:])  # int('+005') = 5
  while exponent % 3:
    digits *= 10
    exponent -= 1
  while exponent > 6:
    digits *= 10
    exponent -= 1
  while exponent < -3:
    digits /= 10
    exponent += 1
  if digits >= 100:
    # Don't append a meaningless '.0' to an integer number.
    digits = int(digits)
  # Exponent is now divisible by 3, between -3 and 6 inclusive: (-3, 0, 3, 6).
  return '%s%s' % (digits, metric_prefixes[exponent])


def _JoinWithSpacesAndNewLine(words):
  """Joins a list of words together with spaces."""
  return ' '.join(str(w) for w in words) + '\n'
