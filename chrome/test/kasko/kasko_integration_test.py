#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A Windows-only end-to-end integration test for Kasko, Chrome and Crashpad.

This test ensures that the interface between Kasko and Chrome and Crashpad works
as expected. The test causes Kasko to set certain crash keys and invoke a crash
report, which is in turn delivered to a locally hosted test crash server. If the
crash report is received intact with the expected crash keys then all is well.

Note that this test only works against non-component Release and Official builds
of Chrome with Chrome branding, and attempting to use it with anything else will
most likely lead to constant failures.

Typical usage (assuming in root 'src' directory):

- generate project files with the following GYP variables:
    branding=Chrome syzyasan=1 win_z7=0 chromium_win_pch=0
- build the release Chrome binaries:
    ninja -C out\Release chrome.exe
- run the test:
    python chrome/test/kasko/kasko_integration_test.py --chrome-dir=out/Release

Many of the components in this test could be reused in other end-to-end crash
testing. Feel free to open them up for reuse, but please CC chrisha@chromium.org
on any associated reviews or bugs!
"""

import BaseHTTPServer
import cgi
import logging
import os
import optparse
import pywintypes
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time
import uuid
import win32api
import win32com.client
import win32con
import win32event
import win32gui
import win32process


_DEFAULT_TIMEOUT = 10  # Seconds.
_LOGGER = logging.getLogger(os.path.basename(__file__))


class _TimeoutException(Exception):
  """Exception used to indicate a timeout has occurred."""
  pass


class _StoppableHTTPServer(BaseHTTPServer.HTTPServer):
  """An extension of BaseHTTPServer that uses timeouts and is interruptable."""

  def server_bind(self):
    BaseHTTPServer.HTTPServer.server_bind(self)
    self.socket.settimeout(1)
    self.run_ = True

  def get_request(self):
    while self.run_:
      try:
        sock, addr = self.socket.accept()
        sock.settimeout(None)
        return (sock, addr)
      except socket.timeout:
        pass

  def stop(self):
    self.run_ = False

  def serve(self):
    while self.run_:
      self.handle_request()


class _CrashServer(object):
  """A simple crash server for testing."""

  def __init__(self):
    self.server_ = None
    self.lock_ = threading.Lock()
    self.crashes_ = []  # Under lock_.

  def crash(self, index):
    """Accessor for the list of crashes."""
    with self.lock_:
      if index >= len(self.crashes_):
        return None
      return self.crashes_[index]

  @property
  def port(self):
    """Returns the port associated with the server."""
    if not self.server_:
      return 0
    return self.server_.server_port

  def start(self):
    """Starts the server on another thread. Call from main thread only."""
    page_handler = self.multipart_form_handler()
    self.server_ = _StoppableHTTPServer(('127.0.0.1', 0), page_handler)
    self.thread_ = self.server_thread()
    self.thread_.start()

  def stop(self):
    """Stops the running server. Call from main thread only."""
    self.server_.stop()
    self.thread_.join()
    self.server_ = None
    self.thread_ = None

  def wait_for_report(self, timeout):
    """Waits until the server has received a crash report.

    Returns True if the a report has been received in the given time, or False
    if a timeout occurred. Since Python condition variables have no notion of
    timeout this is, sadly, a busy loop on the calling thread.
    """
    started = time.time()
    elapsed = 0
    while elapsed < timeout:
      with self.lock_:
        if len(self.crashes_):
          return True
      time.sleep(0.1)
      elapsed = time.time() - started

    return False


  def multipart_form_handler(crash_server):
    """Returns a multi-part form handler class for use with a BaseHTTPServer."""

    class MultipartFormHandler(BaseHTTPServer.BaseHTTPRequestHandler):
      """A multi-part form handler that processes crash reports.

      This class only handles multipart form POST messages, with all other
      requests by default returning a '501 not implemented' error.
      """

      def __init__(self, request, client_address, socket_server):
        BaseHTTPServer.BaseHTTPRequestHandler.__init__(
          self, request, client_address, socket_server)

      def log_message(self, format, *args):
        _LOGGER.debug(format, *args)

      def do_POST(self):
        """Handles POST messages contained multipart form data."""
        content_type, parameters = cgi.parse_header(
            self.headers.getheader('content-type'))
        if content_type != 'multipart/form-data':
          raise Exception('Unsupported Content-Type: ' + content_type)
        post_multipart = cgi.parse_multipart(self.rfile, parameters)

        # Save the crash report.
        report = dict(post_multipart.items())
        report_id = str(uuid.uuid4())
        report['report-id'] = [report_id]
        with crash_server.lock_:
          crash_server.crashes_.append(report)

        # Send the response.
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(report_id)

    return MultipartFormHandler

  def server_thread(crash_server):
    """Returns a thread that hosts the webserver."""

    class ServerThread(threading.Thread):
      def run(self):
        crash_server.server_.serve()

    return ServerThread()


class _ScopedTempDir(object):
  """A class that creates a scoped temporary directory."""

  def __init__(self):
    self.path_ = None

  def __enter__(self):
    """Creates the temporary directory and initializes |path|."""
    self.path_ = tempfile.mkdtemp(prefix='kasko_integration_')
    return self

  def __exit__(self, *args, **kwargs):
    """Destroys the temporary directory."""
    if self.path_ is None:
      return
    shutil.rmtree(self.path_)

  @property
  def path(self):
    return self.path_

  def release(self):
    path = self.path_
    self.path_ = None
    return path


class _ScopedStartStop(object):
  """Utility class for calling 'start' and 'stop' within a scope."""

  def __init__(self, service, start=None, stop=None):
    self.service_ = service

    if start is None:
      self.start_ = lambda x: x.start()
    else:
      self.start_ = start

    if stop is None:
      self.stop_ = lambda x: x.stop()
    else:
      self.stop_ = stop

  def __enter__(self):
    self.start_(self.service_)
    return self

  def __exit__(self, *args, **kwargs):
    if self.service_:
      self.stop_(self.service_)

  @property
  def service(self):
    """Returns the encapsulated service, retaining ownership."""
    return self.service_

  def release(self):
    """Relinquishes ownership of the encapsulated service and returns it."""
    service = self.service_
    self.service_ = None
    return service


def _FindChromeProcessId(user_data_dir, timeout=_DEFAULT_TIMEOUT):
  """Finds the process ID of a given Chrome instance."""
  udd = os.path.abspath(user_data_dir)

  # Find the message window.
  started = time.time()
  elapsed = 0
  msg_win = None
  while msg_win is None:
    try:
      win = win32gui.FindWindowEx(None, None, 'Chrome_MessageWindow', udd)
      if win != 0:
        msg_win = win
        break
    except pywintypes.error:
      continue

    time.sleep(0.1)
    elapsed = time.time() - started
    if elapsed >= timeout:
      raise _TimeoutException()

  # Get the process ID associated with the message window.
  tid, pid = win32process.GetWindowThreadProcessId(msg_win)

  return pid


def _ShutdownProcess(process_id, timeout, force=False):
  """Attempts to nicely close the specified process.

  Returns the exit code on success. Raises an error on failure.
  """

  # Open the process in question, so we can wait for it to exit.
  permissions = win32con.SYNCHRONIZE | win32con.PROCESS_QUERY_INFORMATION
  process_handle = win32api.OpenProcess(permissions, False, process_id)

  # Loop around to periodically retry to close Chrome.
  started = time.time()
  elapsed = 0
  while True:
    _LOGGER.debug('Shutting down process with PID=%d.', process_id)

    with open(os.devnull, 'w') as f:
      cmd = ['taskkill.exe', '/PID', str(process_id)]
      if force:
        cmd.append('/F')
      subprocess.call(cmd, shell=True, stdout=f, stderr=f)

    # Wait at most 2 seconds after each call to taskkill.
    curr_timeout_ms = int(max(2, timeout - elapsed) * 1000)

    _LOGGER.debug('Waiting for process with PID=%d to exit.', process_id)
    result = win32event.WaitForSingleObject(process_handle, curr_timeout_ms)
    # Exit the loop on successful wait.
    if result == win32event.WAIT_OBJECT_0:
      break

    elapsed = time.time() - started
    if elapsed > timeout:
      _LOGGER.debug('Timeout waiting for process to exit.')
      raise _TimeoutException()

  exit_status = win32process.GetExitCodeProcess(process_handle)
  process_handle.Close()
  _LOGGER.debug('Process exited with status %d.', exit_status)

  return exit_status


def _WmiTimeToLocalEpoch(wmitime):
  """Converts a WMI time string to a Unix epoch time."""
  # The format of WMI times is: yyyymmddHHMMSS.xxxxxx[+-]UUU, where
  # UUU is the number of minutes between local time and UTC.
  m = re.match('^(?P<year>\d{4})(?P<month>\d{2})(?P<day>\d{2})'
                   '(?P<hour>\d{2})(?P<minutes>\d{2})(?P<seconds>\d{2}\.\d+)'
                   '(?P<offset>[+-]\d{3})$', wmitime)
  if not m:
    raise Exception('Invalid WMI time string.')

  # This parses the time as a local time.
  t = time.mktime(time.strptime(wmitime[0:14], '%Y%m%d%H%M%S'))

  # Add the fractional part of the seconds that wasn't parsed by strptime.
  s = float(m.group('seconds'))
  t += s - int(s)

  return t


def _GetProcessCreationDate(pid):
  """Returns the process creation date as local unix epoch time."""
  wmi = win32com.client.GetObject('winmgmts:')
  procs = wmi.ExecQuery(
      'select CreationDate from Win32_Process where ProcessId = %s' % pid)
  for proc in procs:
    return _WmiTimeToLocalEpoch(proc.Properties_('CreationDate').Value)
  raise Exception('Unable to find process with PID %d.' % pid)


def _ShutdownChildren(parent_pid, child_exe, started_after, started_before,
                     timeout=_DEFAULT_TIMEOUT, force=False):
  """Shuts down any lingering child processes of a given parent.

  This is an inherently racy thing to do as process IDs are aggressively reused
  on Windows. Filtering by a valid known |started_after| and |started_before|
  timestamp, as well as by the executable of the child process resolves this
  issue. Ugh.
  """
  started = time.time()
  wmi = win32com.client.GetObject('winmgmts:')
  _LOGGER.debug('Shutting down lingering children processes.')
  for proc in wmi.InstancesOf('Win32_Process'):
    if proc.Properties_('ParentProcessId').Value != parent_pid:
      continue
    if proc.Properties_('ExecutablePath').Value != child_exe:
      continue
    t = _WmiTimeToLocalEpoch(proc.Properties_('CreationDate').Value)
    if t <= started_after or t >= started_before:
      continue
    pid = proc.Properties_('ProcessId').Value
    remaining = max(0, started + timeout - time.time())
    _ShutdownProcess(pid, remaining, force=force)


class _ChromeInstance(object):
  """A class encapsulating a running instance of Chrome for testing."""

  def __init__(self, chromedriver, chrome, user_data_dir):
    self.chromedriver_ = os.path.abspath(chromedriver)
    self.chrome_ = os.path.abspath(chrome)
    self.user_data_dir_ = user_data_dir

  def start(self, timeout=_DEFAULT_TIMEOUT):
    capabilities = {
          'chromeOptions': {
            'args': [
              # This allows automated navigation to chrome:// URLs.
              '--enable-gpu-benchmarking',
              '--user-data-dir=%s' % self.user_data_dir_,
            ],
            'binary': self.chrome_,
          }
        }

    # Use a _ScopedStartStop helper so the service and driver clean themselves
    # up in case of any exceptions.
    _LOGGER.info('Starting chromedriver')
    with _ScopedStartStop(service.Service(self.chromedriver_)) as \
        scoped_service:
      _LOGGER.info('Starting chrome')
      with _ScopedStartStop(webdriver.Remote(scoped_service.service.service_url,
                                            capabilities),
                           start=lambda x: None, stop=lambda x: x.quit()) as \
          scoped_driver:
        self.pid_ = _FindChromeProcessId(self.user_data_dir_, timeout)
        self.started_at_ = _GetProcessCreationDate(self.pid_)
        _LOGGER.debug('Chrome launched.')
        self.driver_ = scoped_driver.release()
        self.service_ = scoped_service.release()


  def stop(self, timeout=_DEFAULT_TIMEOUT):
    started = time.time()
    self.driver_.quit()
    self.stopped_at_ = time.time()
    self.service_.stop()
    self.driver_ = None
    self.service = None

    # Ensure that any lingering children processes are torn down as well. This
    # is generally racy on Windows, but is gated based on parent process ID,
    # child executable, and start time of the child process. These criteria
    # ensure we don't go indiscriminately killing processes.
    remaining = max(0, started + timeout - time.time())
    _ShutdownChildren(self.pid_, self.chrome_, self.started_at_,
                      self.stopped_at_, remaining, force=True)

  def navigate_to(self, url):
    """Navigates the running Chrome instance to the provided URL."""
    self.driver_.get(url)


def _ParseCommandLine():
  """Parses the command-line and returns an options structure."""
  self_dir = os.path.dirname(__file__)
  src_dir = os.path.abspath(os.path.join(self_dir, '..', '..', '..'))

  option_parser = optparse.OptionParser()
  option_parser.add_option('--chrome', dest='chrome', type='string',
      default=os.path.join(src_dir, 'out', 'Release', 'chrome.exe'),
      help='Path to chrome.exe. Defaults to $SRC/out/Release/chrome.exe')
  option_parser.add_option('--chromedriver', dest='chromedriver',
      type='string', help='Path to the chromedriver.exe. By default will look '
      'alongside chrome.exe.')
  option_parser.add_option('--keep-temp-dirs', action='store_true',
      default=False, help='Prevents temporary directories from being deleted.')
  option_parser.add_option('--quiet', dest='log_level', action='store_const',
      default=logging.INFO, const=logging.ERROR,
      help='Disables all output except for errors.')
  option_parser.add_option('--user-data-dir', dest='user_data_dir',
      type='string', help='User data directory to use. Defaults to using a '
      'temporary one.')
  option_parser.add_option('--verbose', dest='log_level', action='store_const',
      default=logging.INFO, const=logging.DEBUG,
      help='Enables verbose logging.')
  option_parser.add_option('--webdriver', type='string',
      default=os.path.join(src_dir, 'third_party', 'webdriver', 'pylib'),
      help='Specifies the directory where the python installation of webdriver '
           '(selenium) can be found. Specify an empty string to use the system '
           'installation. Defaults to $SRC/third_party/webdriver/pylib')
  options, args = option_parser.parse_args()
  if args:
    option_parser.error('Unexpected arguments: %s' % args)

  # Validate chrome.exe exists.
  if not os.path.isfile(options.chrome):
    option_parser.error('chrome.exe not found')

  # Use default chromedriver.exe if necessary, and validate it exists.
  if not options.chromedriver:
    options.chromedriver = os.path.join(os.path.dirname(options.chrome),
                                        'chromedriver.exe')
  if not os.path.isfile(options.chromedriver):
    option_parser.error('chromedriver.exe not found')

  # If specified, ensure the webdriver parameters is a directory.
  if options.webdriver and not os.path.isdir(options.webdriver):
    option_parser.error('Invalid webdriver directory.')

  # Configure logging.
  logging.basicConfig(level=options.log_level)

  _LOGGER.debug('Using chrome path: %s', options.chrome)
  _LOGGER.debug('Using chromedriver path: %s', options.chromedriver)
  _LOGGER.debug('Using webdriver path: %s', options.webdriver)

  # Import webdriver and selenium.
  global webdriver
  global service
  if options.webdriver:
    sys.path.append(options.webdriver)
  from selenium import webdriver
  import selenium.webdriver.chrome.service as service

  return options


def Main():
  options = _ParseCommandLine()

  # Generate a temporary directory for use in the tests.
  with _ScopedTempDir() as temp_dir:
    # Prevent the temporary directory from self cleaning if requested.
    if options.keep_temp_dirs:
      temp_dir_path = temp_dir.release()
    else:
      temp_dir_path = temp_dir.path

    # Use the specified user data directory if requested.
    if options.user_data_dir:
      user_data_dir = options.user_data_dir
    else:
      user_data_dir = os.path.join(temp_dir_path, 'user-data-dir')

    kasko_dir = os.path.join(temp_dir_path, 'kasko')
    os.makedirs(kasko_dir)

    # Launch the test server.
    server = _CrashServer()
    with _ScopedStartStop(server):
      _LOGGER.info('Started server on port %d', server.port)

      # Configure the environment so Chrome can find the test crash server.
      os.environ['KASKO_CRASH_SERVER_URL'] = (
          'http://127.0.0.1:%d/crash' % server.port)

      # Launch Chrome and navigate it to the test URL.
      chrome = _ChromeInstance(options.chromedriver, options.chrome,
                              user_data_dir)
      with _ScopedStartStop(chrome):
        _LOGGER.info('Navigating to Kasko debug URL')
        chrome.navigate_to('chrome://kasko/send-report')

        _LOGGER.info('Waiting for Kasko report')
        if not server.wait_for_report(10):
          raise Exception('No Kasko report received.')

    report = server.crash(0)
    for key in sorted(report.keys()):
      val = report[key][0]
      if (len(val) < 64):
        _LOGGER.debug('Got crashkey "%s": "%s"', key, val)
      else:
        _LOGGER.debug('Got crashkey "%s": ...%d bytes...', key, len(val))

    expected_keys = {
        'kasko-set-crash-key-value-impl': 'SetCrashKeyValueImpl',
        'guid': 'GetCrashKeysForKasko'}
    for expected_key, error in expected_keys.iteritems():
      if expected_key not in report:
        _LOGGER.error('Missing expected "%s" crash key.', expected_key)
        raise Exception('"%s" integration appears broken.' % error)

    return 0


if __name__ == '__main__':
  sys.exit(Main())
