#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic pyauto performance tests.

For tests that need to be run for multiple iterations (e.g., so that average
and standard deviation values can be reported), the default number of iterations
run for each of these tests is specified by |_DEFAULT_NUM_ITERATIONS|.
That value can optionally be tweaked by setting an environment variable
'NUM_ITERATIONS' to a positive integer, representing the number of iterations
to run.

Some tests rely on repeatedly appending tabs to Chrome.  Occasionally, these
automation calls time out, thereby affecting the timing measurements (see issue
crosbug.com/20503).  To work around this, the tests discard timing measurements
that involve automation timeouts.  The value |_DEFAULT_MAX_TIMEOUT_COUNT|
specifies the threshold number of timeouts that can be tolerated before the test
fails.  To tweak this value, set environment variable 'MAX_TIMEOUT_COUNT' to the
desired threshold value.
"""

import BaseHTTPServer
import commands
import logging
import math
import os
import posixpath
import re
import SimpleHTTPServer
import SocketServer
import tempfile
import threading
import time
import timeit
import urllib
import urllib2
import urlparse

import pyauto_functional  # Must be imported before pyauto.
import pyauto

import perf_snapshot
import pyauto_utils
import test_utils
from youtube import YoutubeTestHelper


class BasePerfTest(pyauto.PyUITest):
  """Base class for performance tests."""

  _DEFAULT_NUM_ITERATIONS = 50
  _DEFAULT_MAX_TIMEOUT_COUNT = 10
  _PERF_OUTPUT_MARKER_PRE = '_PERF_PRE_'
  _PERF_OUTPUT_MARKER_POST = '_PERF_POST_'

  def setUp(self):
    """Performs necessary setup work before running each test."""
    self._num_iterations = self._DEFAULT_NUM_ITERATIONS
    if 'NUM_ITERATIONS' in os.environ:
      self._num_iterations = int(os.environ['NUM_ITERATIONS'])
    self._max_timeout_count = self._DEFAULT_MAX_TIMEOUT_COUNT
    if 'MAX_TIMEOUT_COUNT' in os.environ:
      self._max_timeout_count = int(os.environ['MAX_TIMEOUT_COUNT'])
    self._timeout_count = 0
    pyauto.PyUITest.setUp(self)

  def _AppendTab(self, url):
    """Appends a tab and increments a counter if the automation call times out.

    Args:
      url: The string url to which the appended tab should be navigated.
    """
    if not self.AppendTab(pyauto.GURL(url)):
      self._timeout_count += 1

  def _MeasureElapsedTime(self, python_command, num_invocations=1):
    """Measures time (in msec) to execute a python command one or more times.

    Args:
      python_command: A callable.
      num_invocations: An integer number of times to invoke the given command.

    Returns:
      The time required to execute the python command the specified number of
      times, in milliseconds as a float.
    """
    assert callable(python_command)
    def RunCommand():
      for _ in range(num_invocations):
        python_command()
    timer = timeit.Timer(stmt=RunCommand)
    return timer.timeit(number=1) * 1000  # Convert seconds to milliseconds.

  def _AvgAndStdDev(self, values):
    """Computes the average and standard deviation of a set of values.

    Args:
      values: A list of numeric values.

    Returns:
      A 2-tuple of floats (average, standard_deviation).
    """
    avg = 0.0
    std_dev = 0.0
    if values:
      avg = float(sum(values)) / len(values)
      if len(values) > 1:
        temp_vals = [math.pow(x - avg, 2) for x in values]
        std_dev = math.sqrt(sum(temp_vals) / (len(temp_vals) - 1))
    return avg, std_dev

  def _OutputPerfGraphValue(self, description, value):
    """Outputs a performance value to have it graphed on the performance bots.

    Only used for ChromeOS.  The performance bots have a 30-character limit on
    the length of the description for a performance value.  Any characters
    beyond that are truncated before results are stored in the autotest
    database.

    Args:
      description: A string description of the performance value.  The string
                   should be of the form "units_identifier" (for example,
                   "milliseconds_NewTabPage").  This description will be
                   truncated to 30 characters when stored in the autotest
                   database.
      value: A numeric value representing a single performance measurement.
    """
    if self.IsChromeOS():
      if len(description) > 30:
        logging.warning('The description "%s" will be truncated to "%s" '
                        '(length 30) when added to the autotest database.',
                        description, description[:30])
      print '\n%s(\'%s\', %.2f)%s' % (self._PERF_OUTPUT_MARKER_PRE, description,
                                      value, self._PERF_OUTPUT_MARKER_POST)

  def _PrintSummaryResults(self, description, values, units):
    """Logs summary measurement information.

    This function computes and outputs the average and standard deviation of
    the specified list of value measurements.  It also invokes
    _OutputPerfGraphValue() with the computed *average* value, to ensure the
    average value can be plotted in a performance graph.

    Args:
      description: A string description for the specified results.
      values: A list of numeric value measurements.
      units: A string specifying the units for the specified measurements.
    """
    logging.info('Results for: ' + description)
    if values:
      avg, std_dev = self._AvgAndStdDev(values)
      logging.info('Number of iterations: %d', len(values))
      for val in values:
        logging.info('  %.2f %s', val, units)
      logging.info('  --------------------------')
      logging.info('  Average: %.2f %s', avg, units)
      logging.info('  Std dev: %.2f %s', std_dev, units)
      self._OutputPerfGraphValue('%s_%s' % (units, description), avg)
    else:
      logging.info('No results to report.')

  def _RunNewTabTest(self, description, open_tab_command, num_tabs=1):
    """Runs a perf test that involves opening new tab(s).

    This helper function can be called from different tests to do perf testing
    with different types of tabs.  It is assumed that the |open_tab_command|
    will open up a single tab.

    Args:
      description: A string description of the associated tab test.
      open_tab_command: A callable that will open a single tab.
      num_tabs: The number of tabs to open, i.e., the number of times to invoke
                the |open_tab_command|.
    """
    assert callable(open_tab_command)

    timings = []
    for _ in range(self._num_iterations):
      orig_timeout_count = self._timeout_count
      elapsed_time = self._MeasureElapsedTime(open_tab_command,
                                              num_invocations=num_tabs)
      # Only count the timing measurement if no automation call timed out.
      if self._timeout_count == orig_timeout_count:
        timings.append(elapsed_time)
      self.assertTrue(self._timeout_count <= self._max_timeout_count,
                      msg='Test exceeded automation timeout threshold.')
      self.assertEqual(1 + num_tabs, self.GetTabCount(),
                       msg='Did not open %d new tab(s).' % num_tabs)
      for _ in range(num_tabs):
        self.GetBrowserWindow(0).GetTab(1).Close(True)

    self._PrintSummaryResults(description, timings, 'milliseconds')

  def _LoginToGoogleAccount(self):
    """Logs in to a testing Google account."""
    creds = self.GetPrivateInfo()['test_google_account']
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    self.NavigateToURL('about:blank')  # Clear the existing tab.


class TabPerfTest(BasePerfTest):
  """Tests that involve opening tabs."""

  def testNewTab(self):
    """Measures time to open a new tab."""
    self._RunNewTabTest('NewTabPage',
                        lambda: self._AppendTab('chrome://newtab'))

  def testNewTabPdf(self):
    """Measures time to open a new tab navigated to a PDF file."""
    self.assertTrue(
        os.path.exists(os.path.join(self.DataDir(), 'pyauto_private', 'pdf',
                                    'TechCrunch.pdf')),
        msg='Missing required PDF data file.')
    url = self.GetFileURLForDataPath('pyauto_private', 'pdf', 'TechCrunch.pdf')
    self._RunNewTabTest('NewTabPdfPage', lambda: self._AppendTab(url))

  def testNewTabFlash(self):
    """Measures time to open a new tab navigated to a flash page."""
    self.assertTrue(
        os.path.exists(os.path.join(self.DataDir(), 'plugin', 'flash.swf')),
        msg='Missing required flash data file.')
    url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self._RunNewTabTest('NewTabFlashPage', lambda: self._AppendTab(url))

  def test20Tabs(self):
    """Measures time to open 20 tabs."""
    self._RunNewTabTest('20TabsNewTabPage',
                        lambda: self._AppendTab('chrome://newtab'), num_tabs=20)


class BenchmarkPerfTest(BasePerfTest):
  """Benchmark performance tests."""

  def testV8BenchmarkSuite(self):
    """Measures score from v8 benchmark suite."""
    url = self.GetFileURLForDataPath('v8_benchmark_v6', 'run.html')

    def _RunBenchmarkOnce(url):
      """Runs the v8 benchmark suite once and returns the results in a dict."""
      self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                      msg='Failed to append tab for v8 benchmark suite.')
      js_done = """
          var val = document.getElementById("status").innerHTML;
          window.domAutomationController.send(val);
      """
      self.assertTrue(
          self.WaitUntil(
              lambda: 'Score:' in self.ExecuteJavascript(js_done, tab_index=1),
              timeout=300, expect_retval=True, retry_sleep=1),
          msg='Timed out when waiting for v8 benchmark score.')

      js_get_results = """
          var result = {};
          result['final_score'] = document.getElementById("status").innerHTML;
          result['all_results'] = document.getElementById("results").innerHTML;
          window.domAutomationController.send(JSON.stringify(result));
      """
      results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
      score_pattern = '(\w+): (\d+)'
      final_score = re.search(score_pattern, results['final_score']).group(2)
      result_dict = {'final_score': int(final_score)}
      for match in re.finditer(score_pattern, results['all_results']):
        benchmark_name = match.group(1)
        benchmark_score = match.group(2)
        result_dict[benchmark_name] = int(benchmark_score)
      self.GetBrowserWindow(0).GetTab(1).Close(True)
      return result_dict

    timings = {}
    for _ in xrange(self._num_iterations):
      result_dict = _RunBenchmarkOnce(url)
      for key, val in result_dict.items():
        timings.setdefault(key, []).append(val)

    for key, val in timings.items():
      print
      if key == 'final_score':
        self._PrintSummaryResults('V8Benchmark', val, 'score')
      else:
        self._PrintSummaryResults('V8Benchmark-%s' % key, val, 'score')

  def testSunSpider(self):
    """Runs the SunSpider javascript benchmark suite."""
    url = self.GetFileURLForDataPath('sunspider', 'sunspider-driver.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for SunSpider benchmark suite.')

    js_is_done = """
        var done = false;
        if (document.getElementById("console"))
          done = true;
        window.domAutomationController.send(JSON.stringify(done));
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_is_done, tab_index=1) == 'true',
            timeout=300, retry_sleep=1),
        msg='Timed out when waiting for SunSpider benchmark score.')

    js_get_results = """
        window.domAutomationController.send(
            document.getElementById("console").innerHTML);
    """
    # Append '<br>' to the result to simplify regular expression matching.
    results = self.ExecuteJavascript(js_get_results, tab_index=1) + '<br>'
    total = re.search('Total:\s*([\d.]+)ms', results).group(1)
    logging.info('Total: %.2f ms' % float(total))
    self._OutputPerfGraphValue('ms_SunSpider-total', float(total))

    for match_category in re.finditer('\s\s(\w+):\s*([\d.]+)ms.+?<br><br>',
                                      results):
      category_name = match_category.group(1)
      category_result = match_category.group(2)
      logging.info('Benchmark "%s": %.2f ms', category_name,
                   float(category_result))
      self._OutputPerfGraphValue('ms_SunSpider-%s' % category_name,
                                 float(category_result))
      for match_result in re.finditer('<br>\s\s\s\s([\w-]+):\s*([\d.]+)ms',
                                      match_category.group(0)):
        result_name = match_result.group(1)
        result_value = match_result.group(2)
        logging.info('  Result "%s-%s": %.2f ms', category_name, result_name,
                     float(result_value))
        self._OutputPerfGraphValue(
            'ms_SunSpider-%s-%s' % (category_name, result_name),
            float(result_value))

  def testDromaeoSuite(self):
    """Measures results from Dromaeo benchmark suite."""
    url = self.GetFileURLForDataPath('dromaeo', 'index.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(url + '?dromaeo')),
                    msg='Failed to append tab for Dromaeo benchmark suite.')

    js_is_ready = """
        var val = document.getElementById('pause').value;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_is_ready, tab_index=1),
            timeout=30, expect_retval='Run', retry_sleep=1),
        msg='Timed out when waiting for Dromaeo benchmark to load.')

    js_run = """
        $('#pause').val('Run').click();
        window.domAutomationController.send('done');
    """
    self.ExecuteJavascript(js_run, tab_index=1)

    js_is_done = """
        var val = document.getElementById('timebar').innerHTML;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: 'Total' in self.ExecuteJavascript(js_is_done, tab_index=1),
            timeout=900, expect_retval=True, retry_sleep=2),
        msg='Timed out when waiting for Dromaeo benchmark to complete.')

    js_get_results = """
        var result = {};
        result['total_result'] = $('#timebar strong').html();
        result['all_results'] = {};
        $('.result-item.done').each(function (i) {
            var group_name = $(this).find('.test b').html().replace(':', '');
            var group_results = {};
            group_results['result'] =
                $(this).find('span').html().replace('runs/s', '')

            group_results['sub_groups'] = {}
            $(this).find('li').each(function (i) {
                var sub_name = $(this).find('b').html().replace(':', '');
                group_results['sub_groups'][sub_name] =
                    $(this).text().match(/: ([\d.]+)/)[1]
            });
            result['all_results'][group_name] = group_results;
        });
        window.domAutomationController.send(JSON.stringify(result));
    """
    results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
    total_result = results['total_result']
    logging.info('Total result: ' + total_result)
    self._OutputPerfGraphValue('runsPerSec_Dromaeo-total', float(total_result))

    for group_name, group in results['all_results'].iteritems():
      logging.info('Benchmark "%s": %s', group_name, group['result'])
      self._OutputPerfGraphValue(
          'runsPerSec_Dromaeo-%s' % group_name.replace(' ', ''),
          float(group['result']))

      for benchmark_name, benchmark_score in group['sub_groups'].iteritems():
        logging.info('  Result "%s": %s', benchmark_name, benchmark_score)


class LiveWebappLoadTest(BasePerfTest):
  """Tests that involve performance measurements of live webapps.

  These tests connect to live webpages (e.g., Gmail, Calendar, Docs) and are
  therefore subject to network conditions.  These tests are meant to generate
  "ball-park" numbers only (to see roughly how long things take to occur from a
  user's perspective), and are not expected to be precise.
  """

  def testNewTabGmail(self):
    """Measures time to open a tab to a logged-in Gmail account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the substring 'Last account activity:'.
    """
    EXPECTED_SUBSTRING = 'Last account activity:'

    def _SubstringExistsOnPage():
      js = """
          var frame = document.getElementById("canvas_frame");
          var divs = frame.contentDocument.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML.indexOf("%s") >= 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleGmailTabOpen():
      self._AppendTab('http://www.gmail.com')
      self.assertTrue(self.WaitUntil(_SubstringExistsOnPage, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Gmail string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabGmail', _RunSingleGmailTabOpen)

  def testNewTabCalendar(self):
    """Measures time to open a tab to a logged-in Calendar account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the calendar print button (title 'Print my calendar').
    """
    EXPECTED_SUBSTRING = 'Print my calendar'

    def _DivTitleStartsWith():
      js = """
          var divs = document.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].hasOwnProperty("title") &&
                divs[i].title.indexOf("%s") == 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleCalendarTabOpen():
      self._AppendTab('http://calendar.google.com')
      self.assertTrue(self.WaitUntil(_DivTitleStartsWith, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Calendar string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabCalendar', _RunSingleCalendarTabOpen)

  def testNewTabDocs(self):
    """Measures time to open a tab to a logged-in Docs account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the expected substring 'last modified' (case insensitive).
    """
    EXPECTED_SUBSTRING = 'last modified'

    def _SubstringExistsOnPage():
      js = """
          var divs = document.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML.toLowerCase().indexOf("%s") >= 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleDocsTabOpen():
      self._AppendTab('http://docs.google.com')
      self.assertTrue(self.WaitUntil(_SubstringExistsOnPage, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Docs string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabDocs', _RunSingleDocsTabOpen)


class YoutubePerfTest(BasePerfTest, YoutubeTestHelper):
  """Test Youtube video performance."""

  def __init__(self, methodName='runTest', **kwargs):
    pyauto.PyUITest.__init__(self, methodName, **kwargs)
    YoutubeTestHelper.__init__(self, self)

  def GetCPUUsage(self):
    """Returns machine's CPU usage
       
    It uses /proc/stat to count CPU usage.
    Returns:
      A dictionary with user, nice, system and idle values.
      Sample dictionary :
      {
        'user': 254544,
        'nice': 9,
        'system': 254768,
        'idle': 2859878,
      }
    """
    cpu = open('/proc/stat').readline().split()
    return {
        'user': int(cpu[1]),
        'nice': int(cpu[2]),
        'system': int(cpu[3]),
        'idle': int(cpu[4])
        }

  def VerifyVideoTotalBytes(self):
    """Returns true if video total bytes information is available"""
    return self.GetVideoTotalBytes() > 0

  def VerifyVideoLoadedBytes(self):
    """Returns true if video loaded bytes information is available"""
    return self.GetVideoLoadedBytes() > 0

  def StartVideoForPerformance(self):
    """Start the test video with all required buffering"""
    self.PlayVideoAndAssert()
    self.ExecuteJavascript("""
        ytplayer.setPlaybackQuality('hd720');
        window.domAutomationController.send('');
    """)
    self.AssertPlayingState()
    self.assertTrue(
        self.WaitUntil(self.VerifyVideoTotalBytes, expect_retval=True),
        msg='Failed to get video total bytes information.')
    self.assertTrue(
        self.WaitUntil(self.VerifyVideoLoadedBytes, expect_retval=True), 
        msg='Failed to get video loaded bytes information')
    loaded_video_bytes = self.GetVideoLoadedBytes()
    total_video_bytes = self.GetVideoTotalBytes()
    self.PauseVideo()
    count = 0
    while total_video_bytes > loaded_video_bytes:
      loaded_video_bytes = self.GetVideoLoadedBytes()
      time.sleep(1)
      count = count + 1
    self.PlayVideo()
    # Ignoring first 10 seconds of video playing so we get smooth
    # videoplayback.
    time.sleep(10)

  def testYoutubeDroppedFrames(self):
    """Measures the Youtube video dropped frames. Runs for 60 secs."""
    self.StartVideoForPerformance()
    init_dropped_frames = self.GetVideoDroppedFrames()
    total_dropped_frames = 0
    dropped_fps = []
    for _ in xrange(60):
      frames = self.GetVideoDroppedFrames() - init_dropped_frames
      current_dropped_frames = frames - total_dropped_frames 
      dropped_fps.append(current_dropped_frames)
      total_dropped_frames = frames
      # Play the video for some time
      time.sleep(1)
    self._PrintSummaryResults('YoutubeDroppedFrames', dropped_fps, 'frames')

  def testYoutubeCPU(self):
    """Measure the Youtube video CPU usage. Runs for 60 seconds.

    Measures the Youtube video CPU usage (between 0 and 1) extrapolated to
    totalframes in the video by taking dropped frames into account. For smooth
    videoplayback this number should be < 0.5..1.0 on a hyperthreaded CPU.
    """
    self.StartVideoForPerformance()
    init_dropped_frames = self.GetVideoDroppedFrames()
    cpu_usage1 = self.GetCPUUsage() 
    total_shown_frames = 0
    for _ in xrange(60):
      total_shown_frames = total_shown_frames + self.GetVideoFrames() 
      # Play the video for some time
      time.sleep(1)
    total_dropped_frames = self.GetVideoDroppedFrames() - init_dropped_frames
    cpu_usage2 = self.GetCPUUsage() 
    
    x2 = cpu_usage2['user'] + cpu_usage2['nice'] + cpu_usage2['system']
    x1 = cpu_usage1['user'] + cpu_usage1['nice'] + cpu_usage1['system']
    y2 = cpu_usage2['user'] + cpu_usage2['nice'] + \
         cpu_usage2['system'] + cpu_usage2['idle']
    y1 = cpu_usage1['user'] + cpu_usage1['nice'] + \
         cpu_usage1['system'] + cpu_usage1['idle']
    total_frames = total_shown_frames + total_dropped_frames
    # Counting extrapolation for utilization to play the video
    self._PrintSummaryResults('YoutubeCPUExtrapolation',
        [((float(x2) - x1) / (y2 - y1)) * total_frames/total_shown_frames],
        'extrapolation')


class WebGLTest(BasePerfTest):
  """Tests for WebGL performance."""

  def _RunWebGLTest(self, url, description):
    """Measures FPS using a specified WebGL demo.

    Args:
      url: The string URL that, once loaded, will run the WebGL demo (default
           WebGL demo settings are used, since this test does not modify any
           settings in the demo).
      description: A string description for this demo, used as a performance
                   value description.  Should not contain any spaces.
    """
    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for %s.' % description)

    get_fps_js = """
      var fps_field = document.getElementById("fps");
      var result = -1;
      if (fps_field)
        result = fps_field.innerHTML;
      window.domAutomationController.send(JSON.stringify(result));
    """

    # Wait until we start getting FPS values.
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(get_fps_js, tab_index=1) != '-1',
            timeout=300, retry_sleep=1),
        msg='Timed out when waiting for FPS values to be available.')

    # Let the experiment run for 5 seconds before we start collecting perf
    # measurements.
    time.sleep(5)

    # Collect the current FPS value each second for the next 30 seconds.  The
    # final result of this test will be the average of these FPS values.
    fps_vals = []
    for _ in xrange(30):
      fps = self.ExecuteJavascript(get_fps_js, tab_index=1)
      fps_vals.append(float(fps.replace('"', '')))
      time.sleep(1)
    self._PrintSummaryResults(description, fps_vals, 'fps')

  def testWebGLAquarium(self):
    """Measures performance using the WebGL Aquarium demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'aquarium',
                                   'aquarium.html'),
        'WebGLAquarium')

  def testWebGLField(self):
    """Measures performance using the WebGL Field demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'field',
                                   'field.html'),
        'WebGLField')

  def testWebGLSpaceRocks(self):
    """Measures performance using the WebGL SpaceRocks demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'spacerocks',
                                   'spacerocks.html'),
        'WebGLSpaceRocks')


class FileUploadDownloadTest(BasePerfTest):
  """Tests that involve measuring performance of upload and download."""

  def setUp(self):
    """Performs necessary setup work before running each test in this class."""
    self._temp_dir = tempfile.mkdtemp()
    self._test_server = PerfTestServer(self._temp_dir)
    self._test_server_port = self._test_server.GetPort()
    self._test_server.Run()
    self.assertTrue(self.WaitUntil(self._IsTestServerRunning),
                    msg='Failed to start local performance test server.')
    BasePerfTest.setUp(self)

  def tearDown(self):
    """Performs necessary cleanup work after running each test in this class."""
    BasePerfTest.tearDown(self)
    self._test_server.ShutDown()
    pyauto_utils.RemovePath(self._temp_dir)

  def _IsTestServerRunning(self):
    """Determines whether the local test server is ready to accept connections.

    Returns:
      True, if a connection can be made to the local performance test server, or
      False otherwise.
    """
    conn = None
    try:
      conn = urllib2.urlopen('http://localhost:%d' % self._test_server_port)
      return True
    except IOError, e:
      return False
    finally:
      if conn:
        conn.close()

  def testDownload100MBFile(self):
    """Measures the time to download a 100 MB file from a local server."""
    CREATE_100MB_URL = (
        'http://localhost:%d/create_file_of_size?filename=data&mb=100' %
        self._test_server_port)
    DOWNLOAD_100MB_URL = 'http://localhost:%d/data' % self._test_server_port
    DELETE_100MB_URL = ('http://localhost:%d/delete_file?filename=data' %
                        self._test_server_port)

    # Tell the local server to create a 100 MB file.
    self.NavigateToURL(CREATE_100MB_URL)

    # Cleaning up downloaded files is done in the same way as in downloads.py.
    # We first identify all existing downloaded files, then remove only those
    # new downloaded files that appear during the course of this test.
    download_dir = self.GetDownloadDirectory().value()
    orig_downloads = []
    if os.path.isdir(download_dir):
      orig_downloads = os.listdir(download_dir)

    def _CleanupAdditionalFilesInDir(directory, orig_files):
      """Removes the additional files in the specified directory.

      This function will remove all files from |directory| that are not
      specified in |orig_files|.

      Args:
        directory: A string directory path.
        orig_files: A list of strings representing the original set of files in
                    the specified directory.
      """
      downloads_to_remove = []
      if os.path.isdir(directory):
        downloads_to_remove = [os.path.join(directory, name)
                               for name in os.listdir(directory)
                               if name not in orig_files]
      for file_name in downloads_to_remove:
        pyauto_utils.RemovePath(file_name)

    def _DownloadFile(url):
      self.DownloadAndWaitForStart(url)
      self.WaitForAllDownloadsToComplete(timeout=2 * 60 * 1000)  # 2 minutes.

    timings = []
    for _ in range(self._num_iterations):
      timings.append(
          self._MeasureElapsedTime(
              lambda: _DownloadFile(DOWNLOAD_100MB_URL), num_invocations=1))
      self.SetDownloadShelfVisible(False)
      _CleanupAdditionalFilesInDir(download_dir, orig_downloads)

    self._PrintSummaryResults('Download100MBFile', timings, 'milliseconds')

    # Tell the local server to delete the 100 MB file.
    self.NavigateToURL(DELETE_100MB_URL)

  def testUpload50MBFile(self):
    """Measures the time to upload a 50 MB file to a local server."""
    # TODO(dennisjeffrey): Replace the use of XMLHttpRequest in this test with
    # FileManager automation to select the upload file when crosbug.com/17903
    # is complete.
    START_UPLOAD_URL = (
        'http://localhost:%d/start_upload?mb=50' % self._test_server_port)

    EXPECTED_SUBSTRING = 'Upload complete'

    def _IsUploadComplete():
      js = """
          result = "";
          var div = document.getElementById("upload_result");
          if (div)
            result = div.innerHTML;
          window.domAutomationController.send(result);
      """
      return self.ExecuteJavascript(js).find(EXPECTED_SUBSTRING) >= 0

    def _RunSingleUpload():
      self.NavigateToURL(START_UPLOAD_URL)
      self.assertTrue(
          self.WaitUntil(_IsUploadComplete, timeout=120, expect_retval=True,
                         retry_sleep=0.10),
          msg='Upload failed to complete before the timeout was hit.')

    timings = []
    for _ in range(self._num_iterations):
      timings.append(self._MeasureElapsedTime(_RunSingleUpload))

    self._PrintSummaryResults('Upload50MBFile', timings, 'milliseconds')


class ScrollTest(BasePerfTest):
  """Tests to measure scrolling performance."""

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flag needed by scroll performance tests.
    return super(ScrollTest, self).ExtraChromeFlags() + ['--disable-gpu-vsync']

  def _RunScrollTest(self, url, description):
    """Runs a scroll performance test on the specified webpage.

    Args:
      url: The string url for the webpage on which to run the scroll test.
      description: A string description for the particular test being run.
    """
    scroll_file = os.path.join(self.DataDir(), 'scroll', 'scroll.js')
    with open(scroll_file, 'r') as f:
      scroll_text = f.read()

    def _RunSingleInvocation(url, scroll_text):
      """Runs a single invocation of the scroll test and returns the FPS."""
      self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                      msg='Failed to append tab for webpage.')

      js = """
          %s
          window.domAutomationController.send('done');
      """ % scroll_text
      self.ExecuteJavascript(js, tab_index=1)

      # Poll the webpage until the test is complete.
      def IsTestDone():
        done_js = """
          if (__scrolling_complete)
            window.domAutomationController.send('true');
          else
            window.domAutomationController.send('false');
        """
        return self.ExecuteJavascript(done_js, tab_index=1) == 'true'

      self.assertTrue(
          self.WaitUntil(IsTestDone, timeout=300, expect_retval=True,
                         retry_sleep=1),
          msg='Timed out when waiting for scrolling tests to complete.')

      # Get the scroll test results from the webpage.
      results_js = """
        window.domAutomationController.send(
            JSON.stringify({'fps': __mean_fps}));
      """
      fps = eval(self.ExecuteJavascript(results_js, tab_index=1))['fps']
      self.GetBrowserWindow(0).GetTab(1).Close(True)
      return fps

    fps_vals = [_RunSingleInvocation(url, scroll_text)
                for _ in range(self._num_iterations)]

    self._PrintSummaryResults(description, fps_vals, 'FPS')

  def testBlankPageScroll(self):
    """Runs the scroll test on a blank page."""
    self._RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'blank.html'), 'ScrollBlankPage')

  def testTextScroll(self):
    """Runs the scroll test on a text-filled page."""
    self._RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'text.html'), 'ScrollTextPage')

  def testGooglePlusScroll(self):
    """Runs the scroll test on a Google Plus anonymized page."""
    self._RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'plus.html'),
        'ScrollGooglePlusPage')


class MemoryBloatTest(BasePerfTest):
  """Tests to analyze memory bloat in webapps."""

  def setUp(self):
    BasePerfTest.setUp(self)

    # Set up an object that takes v8 heap snapshots of the first opened tab
    # (index 0).
    self._snapshotter = perf_snapshot.PerformanceSnapshotter()
    self._snapshot_results = []

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome enables remote debugging on port 9222.  This is required to
    # take v8 heap snapshots of tabs in Chrome.
    return (super(MemoryBloatTest, self).ExtraChromeFlags() +
            ['--remote-debugging-port=9222'])

  def _TakeHeapSnapshot(self):
    """Takes a v8 heap snapshot using |self._snapshotter| and stores the result.

    This function will fail the current test if no snapshot can be taken.

    Returns:
      The number of seconds it took to take the heap snapshot.
    """
    start_time = time.time()
    snapshot = self._snapshotter.HeapSnapshot()
    elapsed_time = time.time() - start_time
    self.assertTrue(snapshot, msg='Failed to take a v8 heap snapshot.')
    self._snapshot_results.append(snapshot[0])
    return elapsed_time

  def GmailBloat(self):
    """Interact with Gmail while periodically taking v8 heap snapshots.

    This test is currently not enabled by default.  It must be run manually.
    """
    # The following cannot yet be imported on ChromeOS.
    import selenium.common.exceptions
    from selenium.webdriver.support.ui import WebDriverWait

    # Log into a test Google account and open up Gmail.
    self._LoginToGoogleAccount()
    self.NavigateToURL('http://www.gmail.com')
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(loaded_tab_title.find('Gmail') >= 0,
                    msg='Loaded tab title does not contain "Gmail": "%s"' %
                        loaded_tab_title)

    driver = self.NewWebDriver()
    # Any call to wait.until() will raise an exception if the timeout is hit.
    wait = WebDriverWait(driver, timeout=60)

    def _SwitchToCanvasFrame(driver):
      """Switch the WebDriver to Gmail's 'canvas_frame', if it's available.

      Args:
        driver: A selenium.webdriver.remote.webdriver.WebDriver object.

      Returns:
        True, if the switch to Gmail's 'canvas_frame' is successful, or
        False if not.
      """
      try:
        driver.switch_to_frame('canvas_frame')
        return True
      except selenium.common.exceptions.NoSuchFrameException:
        return False

    def _GetElement(find_by, value):
      """Gets a WebDriver element object from the webpage DOM.

      Args:
        find_by: A callable that queries WebDriver for an element from the DOM.
        value: A string value that can be passed to the |find_by| callable.

      Returns:
        The identified WebDriver element object, if found in the DOM, or
        None if the element cannot be found.
      """
      try:
        return find_by(value)
      except selenium.common.exceptions.NoSuchElementException:
        return None

    # Wait until Gmail's 'canvas_frame' loads and the 'Inbox' link is present.
    # TODO(dennisjeffrey): Check with the Gmail team to see if there's a better
    # way to tell when the webpage is ready for user interaction.
    wait.until(_SwitchToCanvasFrame)  # Raises exception if the timeout is hit.
    # Wait for the inbox to appear.
    wait.until(lambda _: _GetElement(
                   driver.find_element_by_partial_link_text, 'Inbox'))

    # Interact with Gmail for awhile.  Here, we repeat the following sequence of
    # interactions: click the "Compose" button, enter some text into the "To"
    # field, enter some text into the "Subject" field, then click the "Discard"
    # button to discard the message.
    num_iterations = 5
    for i in xrange(num_iterations):
      logging.info('Chrome interaction iteration %d of %d.' % (
                   i + 1, num_iterations))

      compose_button = wait.until(lambda _: _GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="Compose mail"]'))
      compose_button.click()

      to_field = wait.until(lambda _: _GetElement(
                                driver.find_element_by_name, 'to'))
      to_field.send_keys('nobody@nowhere.com')

      subject_field = wait.until(lambda _: _GetElement(
                                     driver.find_element_by_name, 'subject'))
      subject_field.send_keys('This message is about to be discarded')

      discard_button = wait.until(lambda _: _GetElement(
                                      driver.find_element_by_xpath,
                                      '//div[text()="Discard"]'))
      discard_button.click()

      # Wait for the message to be discarded, assumed to be true after the
      # "To" field is removed from the webpage DOM.
      wait.until(lambda _: not _GetElement(
                     driver.find_element_by_name, 'to'))

      logging.info('Taking heap snapshot...')
      sec_to_snapshot = self._TakeHeapSnapshot()
      logging.info('Snapshot taken (%.2f sec).' % sec_to_snapshot)

    # Output the snapshot results.
    assert len(self._snapshot_results) >= 1
    base_timestamp = self._snapshot_results[0]['timestamp']
    for snapshot_info in self._snapshot_results:
      logging.info('Snapshot time: %.2f sec' % (
                   snapshot_info['timestamp'] - base_timestamp))
      logging.info('  Total heap size: %.2f MB' % (
                   snapshot_info['total_heap_size'] / (1024.0 * 1024.0)))
      logging.info('  Total node count: %d nodes' % (
                   snapshot_info['total_node_count']))


class FlashTest(BasePerfTest):
  """Tests to measure flash performance."""

  def _RunFlashTestForAverageFPS(self, webpage_url, description):
    """Runs a single flash test that measures an average FPS value.

    Args:
      webpage_url: The string URL to a webpage that will run the test.
      description: A string description for this test.
    """
    # Open up the test webpage; it's assumed the test will start automatically.
    self.assertTrue(self.AppendTab(pyauto.GURL(webpage_url)),
                    msg='Failed to append tab for webpage.')

    # Wait until the final result is computed, then retrieve and output it.
    js = """
        window.domAutomationController.send(
            JSON.stringify(final_average_fps));
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js, tab_index=1) != '-1',
            timeout=300, expect_retval=True, retry_sleep=1),
        msg='Timed out when waiting for test result.')
    result = float(self.ExecuteJavascript(js, tab_index=1))
    logging.info('Result for %s: %.2f FPS (average)', description, result)
    self._OutputPerfGraphValue('%s_%s' % ('FPS', description), result)

  def testFlashGaming(self):
    """Runs a simple flash gaming benchmark test."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'FlashGamingTest2.html')
    self._RunFlashTestForAverageFPS(webpage_url, 'FlashGaming')

  def testFlashText(self):
    """Runs a simple flash text benchmark test."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'FlashTextTest2.html')
    self._RunFlashTestForAverageFPS(webpage_url, 'FlashText')

  def testScimarkGui(self):
    """Runs the ScimarkGui benchmark tests."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'scimarkGui.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(webpage_url)),
                    msg='Failed to append tab for webpage.')

    js = 'window.domAutomationController.send(JSON.stringify(tests_done));'
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js, tab_index=1), timeout=300,
            expect_retval='true', retry_sleep=1),
        msg='Timed out when waiting for tests to complete.')

    js_result = """
        var result = {};
        for (var i = 0; i < tests_results.length; ++i) {
          var test_name = tests_results[i][0];
          var mflops = tests_results[i][1];
          var mem = tests_results[i][2];
          result[test_name] = [mflops, mem]
        }
        window.domAutomationController.send(JSON.stringify(result));
    """
    result = eval(self.ExecuteJavascript(js_result, tab_index=1))
    for benchmark in result:
      mflops = float(result[benchmark][0])
      mem = float(result[benchmark][1])
      if benchmark.endswith('_mflops'):
        benchmark = benchmark[:benchmark.find('_mflops')]
      logging.info('Results for ScimarkGui_' + benchmark + ':')
      logging.info('  %.2f MFLOPS', mflops)
      logging.info('  %.2f MB', mem)
      self._OutputPerfGraphValue(
          '%s_ScimarkGui-%s-MFLOPS' % ('MFLOPS', benchmark), mflops)
      self._OutputPerfGraphValue(
          '%s_ScimarkGui-%s-Mem' % ('MB', benchmark), mem)


class PerfTestServerRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Request handler for the local performance test server."""

  def _IgnoreHandler(self, unused_args):
    """A GET request handler that simply replies with status code 200.

    Args:
      unused_args: A dictionary of arguments for the current GET request.
                   The arguments are ignored.
    """
    self.send_response(200)
    self.end_headers()

  def _CreateFileOfSizeHandler(self, args):
    """A GET handler that creates a local file with the specified size.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
            contain 'filename' and 'mb' keys that refer to the name of the
            file to create and its desired size, respectively.
    """
    megabytes = None
    filename = None
    try:
      megabytes = int(args['mb'][0])
      filename = args['filename'][0]
    except (ValueError, KeyError, IndexError), e:
      logging.exception('Server error creating file: %s', e)
    assert megabytes and filename
    with open(os.path.join(self.server.docroot, filename), 'wb') as f:
      f.write('X' * 1024 * 1024 * megabytes)
    self.send_response(200)
    self.end_headers()

  def _DeleteFileHandler(self, args):
    """A GET handler that deletes the specified local file.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
            contain a 'filename' key that refers to the name of the file
            to delete, relative to the server's document root.
    """
    filename = None
    try:
      filename = args['filename'][0]
    except (KeyError, IndexError), e:
      logging.exception('Server error deleting file: %s', e)
    assert filename
    try:
      os.remove(os.path.join(self.server.docroot, filename))
    except OSError, e:
      logging.warning('OS error removing file: %s', e)
    self.send_response(200)
    self.end_headers()

  def _StartUploadHandler(self, args):
    """A GET handler to serve a page that uploads the given amount of data.

    When the page loads, the specified amount of data is automatically
    uploaded to the same local server that is handling the current request.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
            contain an 'mb' key that refers to the size of the data to
            upload.
    """
    megabytes = None
    try:
      megabytes = int(args['mb'][0])
    except (ValueError, KeyError, IndexError), e:
      logging.exception('Server error starting upload: %s', e)
    assert megabytes
    script = """
        <html>
          <head>
            <script type='text/javascript'>
              function startUpload() {
                var megabytes = %s;
                var data = Array((1024 * 1024 * megabytes) + 1).join('X');
                var boundary = '***BOUNDARY***';
                var xhr = new XMLHttpRequest();

                xhr.open('POST', 'process_upload', true);
                xhr.setRequestHeader(
                    'Content-Type',
                    'multipart/form-data; boundary="' + boundary + '"');
                xhr.setRequestHeader('Content-Length', data.length);
                xhr.onreadystatechange = function() {
                  if (xhr.readyState == 4 && xhr.status == 200) {
                    document.getElementById('upload_result').innerHTML =
                        xhr.responseText;
                  }
                };
                var body = '--' + boundary + '\\r\\n';
                body += 'Content-Disposition: form-data;' +
                        'file_contents=' + data;
                xhr.send(body);
              }
            </script>
          </head>

          <body onload="startUpload();">
            <div id='upload_result'>Uploading...</div>
          </body>
        </html>
    """ % megabytes
    self.send_response(200)
    self.end_headers()
    self.wfile.write(script)

  def _ProcessUploadHandler(self, form):
    """A POST handler that discards uploaded data and sends a response.

    Args:
      form: A dictionary containing posted form data, as returned by
            urlparse.parse_qs().
    """
    upload_processed = False
    file_size = 0
    if 'file_contents' in form:
      file_size = len(form['file_contents'][0])
      upload_processed = True
    self.send_response(200)
    self.end_headers()
    if upload_processed:
      self.wfile.write('Upload complete (%d bytes)' % file_size)
    else:
      self.wfile.write('No file contents uploaded')

  GET_REQUEST_HANDLERS = {
    'create_file_of_size': _CreateFileOfSizeHandler,
    'delete_file': _DeleteFileHandler,
    'start_upload': _StartUploadHandler,
    'favicon.ico': _IgnoreHandler,
  }

  POST_REQUEST_HANDLERS = {
    'process_upload': _ProcessUploadHandler,
  }

  def translate_path(self, path):
    """Ensures files are served from the given document root.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    path = urlparse.urlparse(path)[2]
    path = posixpath.normpath(urllib.unquote(path))
    words = path.split('/')
    words = filter(None, words)  # Remove empty strings from |words|.
    path = self.server.docroot
    for word in words:
      _, word = os.path.splitdrive(word)
      _, word = os.path.split(word)
      if word in (os.curdir, os.pardir):
        continue
      path = os.path.join(path, word)
    return path

  def do_GET(self):
    """Processes a GET request to the local server.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    split_url = urlparse.urlsplit(self.path)
    base_path = split_url[2]
    if base_path.startswith('/'):
      base_path = base_path[1:]
    args = urlparse.parse_qs(split_url[3])
    if base_path in self.GET_REQUEST_HANDLERS:
      self.GET_REQUEST_HANDLERS[base_path](self, args)
    else:
      SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

  def do_POST(self):
    """Processes a POST request to the local server.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    form = urlparse.parse_qs(
        self.rfile.read(int(self.headers.getheader('Content-Length'))))
    path = urlparse.urlparse(self.path)[2]
    if path.startswith('/'):
      path = path[1:]
    if path in self.POST_REQUEST_HANDLERS:
      self.POST_REQUEST_HANDLERS[path](self, form)
    else:
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('No handler for POST request "%s".' % path)


class ThreadedHTTPServer(SocketServer.ThreadingMixIn,
                         BaseHTTPServer.HTTPServer):
  def __init__(self, server_address, handler_class):
    BaseHTTPServer.HTTPServer.__init__(self, server_address, handler_class)


class PerfTestServer(object):
  """Local server for use by performance tests."""

  def __init__(self, docroot):
    """Initializes the performance test server.

    Args:
      docroot: The directory from which to serve files.
    """
    # The use of 0 means to start the server on an arbitrary available port.
    self._server = ThreadedHTTPServer(('', 0),
                                      PerfTestServerRequestHandler)
    self._server.docroot = docroot
    self._server_thread = threading.Thread(target=self._server.serve_forever)

  def Run(self):
    """Starts the server thread."""
    self._server_thread.start()

  def ShutDown(self):
    """Shuts down the server."""
    self._server.shutdown()
    self._server_thread.join()

  def GetPort(self):
    """Identifies the port number to which the server is currently bound.

    Returns:
      The numeric port number to which the server is currently bound.
    """
    return self._server.server_address[1]


if __name__ == '__main__':
  pyauto_functional.Main()
