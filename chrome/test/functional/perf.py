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
import logging
import math
import os
import posixpath
import SimpleHTTPServer
import SocketServer
import tempfile
import threading
import timeit
import urllib
import urllib2
import urlparse

import pyauto_functional  # Must be imported before pyauto.
import pyauto
import pyauto_utils
import test_utils


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

    Only used for ChromeOS.

    Args:
      description: A string description of the performance value.
      value: A numeric value representing a single performance measurement.
    """
    if self.IsChromeOS():
      print '\n%s(\'%s\', %.2f)%s' % (self._PERF_OUTPUT_MARKER_PRE, description,
                                      value, self._PERF_OUTPUT_MARKER_POST)

  def _PrintSummaryResults(self, description, values, units):
    """Logs summary measurement information.

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
    """Measures score from online v8 benchmark suite."""
    url = self.GetFileURLForDataPath('v8_benchmark_v6', 'run.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for v8 benchmark suite.')
    js = """
        var val = document.getElementById("status").innerHTML;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: 'Score:' in self.ExecuteJavascript(js, 0, 1), timeout=300,
            expect_retval=True, retry_sleep=0.10),
        msg='Timed out when waiting for v8 benchmark score.')
    val = self.ExecuteJavascript(js, 0, 1)  # Window index 0, tab index 1.
    score = int(val.split(':')[1].strip())
    self._PrintSummaryResults('V8Benchmark', [score], 'score')


class LiveWebappLoadTest(BasePerfTest):
  """Tests that involve performance measurements of live webapps.

  These tests connect to live webpages (e.g., Gmail, Calendar, Docs) and are
  therefore subject to network conditions.  These tests are meant to generate
  "ball-park" numbers only (to see roughly how long things take to occur from a
  user's perspective), and are not expected to be precise.
  """

  def _LoginToGoogleAccount(self):
    """Logs in to a testing Google account."""
    creds = self.GetPrivateInfo()['test_google_account']
    test_utils.GoogleAccountsLogin(self, creds['username'], creds['password'])
    self.assertTrue(self.WaitForInfobarCount(1),
                    msg='Save password infobar did not appear when logging in.')
    self.NavigateToURL('about:blank')  # Clear the existing tab.

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
      return self.ExecuteJavascript(js, 0, 1)  # Window index 0, tab index 1.

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
      return self.ExecuteJavascript(js, 0, 1)  # Window index 0, tab index 1.

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
    webpage displays the expected substring 'No item selected'.
    """
    EXPECTED_SUBSTRING = 'No item selected'

    def _SubstringExistsOnPage():
      js = """
          var divs = document.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML.indexOf("%s") >= 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, 0, 1)  # Window index 0, tab index 1.

    def _RunSingleDocsTabOpen():
      self._AppendTab('http://docs.google.com')
      self.assertTrue(self.WaitUntil(_SubstringExistsOnPage, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Docs string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabDocs', _RunSingleDocsTabOpen)


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
      return self.ExecuteJavascript(js, 0, 0).find(EXPECTED_SUBSTRING) >= 0

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

    Extends the default list of extra flags passed to Chrome.  See
    ExtraChromeFlags() in pyauto.py.
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
      self.ExecuteJavascript(js, 0, 1)  # Window index 0, tab index 1.

      # Poll the webpage until the test is complete.
      def IsTestDone():
        done_js = """
          if (__scrolling_complete)
            window.domAutomationController.send('true');
          else
            window.domAutomationController.send('false');
        """
        return self.ExecuteJavascript(done_js, 0, 1) == 'true'

      self.assertTrue(
          self.WaitUntil(IsTestDone, timeout=300, expect_retval=True,
                         retry_sleep=0.25),
          msg='Timed out when waiting for scrolling tests to complete.')

      # Get the scroll test results from the webpage.
      results_js = """
        window.domAutomationController.send("{'fps': " + __mean_fps + "}");
      """
      fps = eval(self.ExecuteJavascript(results_js, 0, 1))['fps']
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
      logging.exception('Server error creating file: %s' % e)
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
      logging.exception('Server error deleting file: %s' % e)
    assert filename
    try:
      os.remove(os.path.join(self.server.docroot, filename))
    except OSError, e:
      logging.warning('OS error removing file: %s' % e)
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
      logging.exception('Server error starting upload: %s' % e)
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
