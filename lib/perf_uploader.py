# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Uploads performance data to the performance dashboard.

Image tests may output data that needs to be displayed on the performance
dashboard.  The image test stage/runner invokes this module with each test
associated with a job.  If a test has performance data associated with it, it
is uploaded to the performance dashboard.  The performance dashboard is owned
by Chrome team and is available here: https://chromeperf.appspot.com/.  Users
must be logged in with an @google.com account to view chromeOS perf data there.

This module is similar to src/third_party/autotest/files/tko/perf_uploader.py.
"""

from __future__ import print_function

import collections
import httplib
import json
import logging
import math
import os
import string # pylint: disable=W0402
import urllib
import urllib2

from chromite.lib import osutils


_ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
_PRESENTATION_CONFIG_FILE = os.path.join(_ROOT_DIR,
                                         'perf_dashboard_config.json')
_DASHBOARD_UPLOAD_URL = 'https://chromeperf.appspot.com/add_point'
#_DASHBOARD_UPLOAD_URL = 'http://localhost:8080/add_point'

_MAX_DESCRIPTION_LENGTH = 256
_MAX_UNIT_LENGTH = 32


class PerfUploadingError(Exception):
  """A dummy class to wrap errors in this module."""
  pass


PerformanceValue = collections.namedtuple('PerformanceValue',
    'description value units higher_is_better graph')


def OutputPerfValue(filename, description, value, units,
                    higher_is_better=True, graph=None):
  """Record a measured performance value in an output file.

  This is originally from autotest/files/client/common_lib/test.py.

  The output file will subsequently be parsed by ImageTestStage to have the
  information sent to chromeperf.appspot.com.

  Args:
    filename: A path to the output file. Data will be appended to this file.
    description: A string describing the measured perf value. Must
      be maximum length 256, and may only contain letters, numbers,
      periods, dashes, and underscores.  For example:
      "page_load_time", "scrolling-frame-rate".
    value: A number representing the measured perf value, or a list of
      measured values if a test takes multiple measurements. Measured perf
      values can be either ints or floats.
    units: A string describing the units associated with the measured perf
      value(s). Must be maximum length 32, and may only contain letters,
      numbers, periods, dashes, and uderscores. For example: "msec", "fps".
    higher_is_better: A boolean indicating whether or not a higher measured
      perf value is considered better. If False, it is assumed that a "lower"
      measured value is better.
    graph: A string indicating the name of the graph on which the perf value
      will be subsequently displayed on the chrome perf dashboard. This
      allows multiple metrics to be grouped together on the same graph.
      Default to None, perf values should be graphed individually on separate
      graphs.
  """
  def ValidateString(param_name, value, max_len):
    if len(value) > max_len:
      raise ValueError('%s must be at most %d characters.', param_name, max_len)

    allowed_chars = string.ascii_letters + string.digits + '-._'
    if not set(value).issubset(set(allowed_chars)):
      raise ValueError(
          '%s may only contain letters, digits, hyphens, periods, and '
          'underscores. Its current value is %s.',
          param_name, value
      )

  ValidateString('description', description, _MAX_DESCRIPTION_LENGTH)
  ValidateString('units', units, _MAX_UNIT_LENGTH)

  entry = {
      'description': description,
      'value': value,
      'units': units,
      'higher_is_better': higher_is_better,
      'graph': graph,
  }

  data = (json.dumps(entry), '\n')
  osutils.WriteFile(filename, data, 'a')


def LoadPerfValues(filename):
  """Return a list of PerformanceValue objects from |filename|."""
  lines = osutils.ReadFile(filename).splitlines()
  entries = []
  for line in lines:
    entry = json.loads(line)
    entries.append(PerformanceValue(**entry))
  return entries


def _AggregateIterations(perf_values):
  """Aggregate same measurements from multiple iterations.

  Each perf measurement may exist multiple times across multiple iterations
  of a test.  Here, the results for each unique measured perf metric are
  aggregated across multiple iterations.

  Args:
    perf_values: A list of PerformanceValue objects.

  Returns:
    A dictionary mapping each unique measured perf value (keyed by tuple of
      its description and graph name) to information about that perf value
      (in particular, the value is a list of values for each iteration).
  """
  aggregated_data = {}
  for perf_value in perf_values:
    key = (perf_value.description, perf_value.graph)
    try:
      aggregated_entry = aggregated_data[key]
    except KeyError:
      aggregated_entry = {
          'units': perf_value.units,
          'higher_is_better': perf_value.higher_is_better,
          'graph': perf_value.graph,
          'value': [],
      }
      aggregated_data[key] = aggregated_entry
    # Note: the stddev will be recomputed later when the results
    # from each of the multiple iterations are averaged together.
    aggregated_entry['value'].append(perf_value.value)
  return aggregated_data


def _MeanAndStddev(data, precision=4):
  """Computes mean and standard deviation from a list of numbers.

  Args:
    data: A list of numeric values.
    precision: The integer number of decimal places to which to
      round the results.

  Returns:
    A 2-tuple (mean, standard_deviation), in which each value is
      rounded to |precision| decimal places.
  """
  n = len(data)
  if n == 0:
    raise ValueError('Cannot compute mean and stddev of an empty list.')
  if n == 1:
    return round(data[0], precision), 0

  mean = math.fsum(data) / n
  # Divide by n-1 to compute "sample standard deviation".
  variance = math.fsum((elem - mean) ** 2 for elem in data) / (n - 1)
  return round(mean, precision), round(math.sqrt(variance), precision)


def _ComputeAvgStddev(perf_data):
  """Compute average and standard deviations as needed for perf measurements.

  For any perf measurement that exists in multiple iterations (has more than
  one measured value), compute the average and standard deviation for it and
  then store the updated information in the dictionary (in place).

  Args:
    perf_data: A dictionary of measured perf data as computed by
      _AggregateIterations(), except each "value" is now a single value, not
      a list of values.
  """
  for perf in perf_data.itervalues():
    perf['value'], perf['stddev'] = _MeanAndStddev(perf['value'])
  return perf_data


PresentationInfo = collections.namedtuple('PresentationInfo',
    'master_name test_name')


def _GetPresentationInfo(test_name):
  """Get presentation info for |test_name| from config file.

  Args:
    test_name: The test name.

  Returns:
    A PresentationInfo object for this test.
  """
  infos = osutils.ReadFile(_PRESENTATION_CONFIG_FILE)
  infos = json.loads(infos)
  for info in infos:
    if info['test_name'] == test_name:
      try:
        return PresentationInfo(**info)
      except:
        raise PerfUploadingError('No master found for %s' % test_name)
  else:
    raise PerfUploadingError('No presentation config found for %s' % test_name)


def _FormatForUpload(perf_data, platform_name, cros_version, chrome_version,
                     presentation_info):
  """Formats perf data suitably to upload to the perf dashboard.

  The perf dashboard expects perf data to be uploaded as a
  specially-formatted JSON string.  In particular, the JSON object must be a
  dictionary with key "data", and value being a list of dictionaries where
  each dictionary contains all the information associated with a single
  measured perf value: master name, bot name, test name, perf value, units,
  and build version numbers.

  See also google3/googleclient/chrome/speed/dashboard/add_point.py for the
  server side handler.

  Args:
    platform_name: The string name of the platform.
    cros_version: The string ChromeOS version number.
    chrome_version: The string Chrome version number.
    perf_data: A dictionary of measured perf data. This is keyed by
      (description, graph name) tuple.
    presentation_info: A PresentationInfo object of the given test.

  Returns:
    A dictionary containing the formatted information ready to upload
      to the performance dashboard.
  """
  dash_entries = []
  for (desc, graph), data in perf_data.iteritems():
    # Each perf metric is named by a path that encodes the test name,
    # a graph name (if specified), and a description.  This must be defined
    # according to rules set by the Chrome team, as implemented in:
    # chromium/tools/build/scripts/slave/results_dashboard.py.
    desc = desc.replace('/', '_')
    if graph:
      test_path = 'cbuildbot.%s/%s/%s' % (presentation_info.test_name,
                                          graph, desc)
    else:
      test_path = 'cbuildbot.%s/%s' % (presentation_info.test_name, desc)

    new_dash_entry = {
        'master': presentation_info.master_name,
        'bot': 'cros-' + platform_name,  # Prefix to clarify it's chromeOS.
        'test': test_path,
        'value': data['value'],
        'error': data['stddev'],
        'units': data['units'],
        'higher_is_better': data['higher_is_better'],
        'supplemental_columns': {
            'r_cros_version': cros_version,
            'r_chrome_version': chrome_version,
        }
    }

    dash_entries.append(new_dash_entry)

  json_string = json.dumps(dash_entries)
  return {'data': json_string}


def _SendToDashboard(data_obj):
  """Sends formatted perf data to the perf dashboard.

  Args:
    data_obj: A formatted data object as returned by _FormatForUpload().

  Raises:
    PerfUploadingError if an exception was raised when uploading.
  """
  encoded = urllib.urlencode(data_obj)
  req = urllib2.Request(_DASHBOARD_UPLOAD_URL, encoded)
  try:
    urllib2.urlopen(req)
  except urllib2.HTTPError as e:
    raise PerfUploadingError('HTTPError: %d %s for JSON %s\n' %
                              (e.code, e.msg, data_obj['data']))
  except urllib2.URLError as e:
    raise PerfUploadingError('URLError: %s for JSON %s\n' %
                             (str(e.reason), data_obj['data']))
  except httplib.HTTPException:
    raise PerfUploadingError('HTTPException for JSON %s\n' % data_obj['data'])


def UploadPerfValues(perf_values, platform_name, cros_version, chrome_version,
                     test_name):
  """Uploads any perf data associated with a test to the perf dashboard.

  Args:
    perf_values: List of PerformanceValue objects.
    platform_name: A string identifying platform e.g. 'x86-release'. 'cros-'
      will be prepended to |platform_name| internally, by _FormatForUpload.
    cros_version: A string identifying Chrome OS version e.g. '6052.0.0'.
    chrome_version: A string identifying Chrome OS version e.g. '38.0.2091.2'.
    test_name: A string identifying the test
  """
  if not perf_values:
    return

  # Aggregate values from multiple iterations together.
  perf_data = _AggregateIterations(perf_values)

  # Compute averages and standard deviations as needed for measured perf
  # values that exist in multiple iterations.  Ultimately, we only upload a
  # single measurement (with standard deviation) for every unique measured
  # perf metric.
  _ComputeAvgStddev(perf_data)

  # Format the perf data for the upload, then upload it.
  # Prefix the ChromeOS version number with the Chrome milestone.
  # TODO(dennisjeffrey): Modify the dashboard to accept the ChromeOS version
  # number *without* the milestone attached.
  cros_version = chrome_version[:chrome_version.find('.') + 1] + cros_version
  try:
    presentation_info = _GetPresentationInfo(test_name)
    formatted_data = _FormatForUpload(perf_data, platform_name,
                                      cros_version, chrome_version,
                                      presentation_info)
    _SendToDashboard(formatted_data)
  except PerfUploadingError:
    logging.exception('Error when uploading perf data to the perf '
                      'dashboard for test %s.', test_name)
    raise
  else:
    logging.info('Successfully uploaded perf data to the perf '
                 'dashboard for test %s.', test_name)
