#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto: Python Interface to Chromium's Automation Proxy.

PyAuto uses swig to expose Automation Proxy interfaces to Python.
For complete documentation on the functionality available,
run pydoc on this file.

Ref: http://dev.chromium.org/developers/testing/pyauto


Include the following in your PyAuto test script to make it run standalone.

from pyauto import Main

if __name__ == '__main__':
  Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto.py test_script
"""

import hashlib
import logging
import optparse
import os
import shutil
import signal
import stat
import subprocess
import sys
import tempfile
import time
import types
import unittest
import urllib


def _LocateBinDirs():
  """Setup a few dirs where we expect to find dependency libraries."""
  script_dir = os.path.dirname(__file__)
  chrome_src = os.path.join(script_dir, os.pardir, os.pardir, os.pardir)

  bin_dirs = {
      'linux2': [ os.path.join(chrome_src, 'out', 'Debug'),
                  os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                  os.path.join(chrome_src, 'out', 'Release'),
                  os.path.join(chrome_src, 'sconsbuild', 'Release')],
      'darwin': [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
                  os.path.join(chrome_src, 'xcodebuild', 'Release')],
      'win32':  [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'build', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release'),
                  os.path.join(chrome_src, 'build', 'Release')],
      'cygwin': [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release')],
  }
  deps_dirs = [ os.path.join(script_dir, os.pardir,
                             os.pardir, os.pardir, 'third_party'),
                script_dir,
  ]
  sys.path += map(os.path.normpath, bin_dirs.get(sys.platform, []) + deps_dirs)

_LocateBinDirs()

try:
  import pyautolib
  # Needed so that all additional classes (like: FilePath, GURL) exposed by
  # swig interface get available in this module.
  from pyautolib import *
except ImportError:
  print >>sys.stderr, "Could not locate built libraries. Did you build?"
  # Mac requires python2.5 even when not the default 'python' (e.g. 10.6)
  if 'darwin' == sys.platform and sys.version_info[:2] != (2,5):
    print  >>sys.stderr, "*\n* Perhaps use 'python2.5', not 'python' ?\n*"
  raise

# Should go after sys.path is set appropriately
import bookmark_model
import download_info
import history_info
import omnibox_info
import plugins_info
import prefs_info
from pyauto_errors import JSONInterfaceError
from pyauto_errors import NTPThumbnailNotShownError
import pyauto_utils
import simplejson as json  # found in third_party

_HTTP_SERVER = None
_OPTIONS = None


class PyUITest(pyautolib.PyUITestBase, unittest.TestCase):
  """Base class for UI Test Cases in Python.

  A browser is created before executing each test, and is destroyed after
  each test irrespective of whether the test passed or failed.

  You should derive from this class and create methods with 'test' prefix,
  and use methods inherited from PyUITestBase (the C++ side).

  Example:

    class MyTest(PyUITest):

      def testNavigation(self):
        self.NavigateToURL("http://www.google.com")
        self.assertEqual("Google", self.GetActiveTabTitle())
  """

  def __init__(self, methodName='runTest', **kwargs):
    """Initialize PyUITest.

    When redefining __init__ in a derived class, make sure that:
      o you make a call this __init__
      o __init__ takes methodName as an arg. this is mandated by unittest module

    Args:
      methodName: the default method name. Internal use by unittest module

      (The rest of the args can be in any order. They can even be skipped in
       which case the defaults will be used.)

      clear_profile: If True, clean the profile dir before use. Defaults to True
      homepage: the home page. Defaults to "about:blank"
    """
    # Fetch provided keyword args, or fill in defaults.
    clear_profile = kwargs.get('clear_profile', True)
    homepage = kwargs.get('homepage', 'about:blank')

    pyautolib.PyUITestBase.__init__(self, clear_profile, homepage)
    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    self.Initialize(pyautolib.FilePath(browser_dir))
    unittest.TestCase.__init__(self, methodName)

  def __del__(self):
    pyautolib.PyUITestBase.__del__(self)

  def setUp(self):
    """Override this method to launch browser differently.

    Can be used to prevent launching the browser window by default in case a
    test wants to do some additional setup before firing browser.

    When using the named interface, it connects to an existing browser
    instance.
    """
    named_channel_id = _OPTIONS.channel_id
    if self.IsChromeOS():  # Enable testing interface on ChromeOS
      if self.get_clear_profile():
        self.CleanupBrowserProfileOnChromeOS()
      self.EnableCrashReportingOnChromeOS()
      if not named_channel_id:
        named_channel_id = self.EnableChromeTestingOnChromeOS()
    if named_channel_id:
      self._named_channel_id = named_channel_id
      self.UseNamedChannelID(named_channel_id)
    self.SetUp()     # Fire browser

    # TODO(dtu): Remove this after crosbug.com/4558 is fixed.
    if self.IsChromeOS():
      self.WaitUntil(lambda: not self.GetNetworkInfo()['offline_mode'])

  def tearDown(self):
    self.TearDown()  # Destroy browser

  @staticmethod
  def CloseChromeOnChromeOS():
    """Gracefully exit chrome on ChromeOS."""
    subprocess.call(['pkill', 'chrome'])

  def EnableChromeTestingOnChromeOS(self):
    """Enables the named automation interface on chromeos.

    Restarts chrome so that you get a fresh instance.
    Also sets some testing-friendly flags for chrome.

    Expects suid python to be present in the same dir as pyautolib.py
    """
    def _IsRootSuid(path):
      return os.path.isfile(path) and (os.stat(path).st_mode & stat.S_ISUID)
    suid_python = os.path.normpath(os.path.join(
        os.path.dirname(pyautolib.__file__), 'suid-python'))
    assert _IsRootSuid(suid_python), \
        'Did not find suid-root python at %s' % suid_python
    file_path = os.path.join(os.path.dirname(__file__), 'chromeos',
                             'enable_testing.py')
    args = [suid_python, file_path]
    # Pass extra chrome flags for testing
    for flag in self.ExtraChromeFlagsOnChromeOS():
      args.append('--extra-chrome-flags=%s' % flag)
    assert self.WaitUntil(lambda: self._IsSessionManagerReady(0))
    proc = subprocess.Popen(args, stdout=subprocess.PIPE)
    automation_channel_path = proc.communicate()[0].strip()
    assert len(automation_channel_path), 'Could not enable testing interface'
    return automation_channel_path

  @staticmethod
  def EnableCrashReportingOnChromeOS():
    """Enables crash reporting on ChromeOS.

    Writes the "/home/chronos/Consent To Send Stats" file with a 32-char
    readable string.  See comment in session_manager_setup.sh which does this
    too.

    Note that crash reporting will work only if breakpad is built in, ie in a
    'Google Chrome' build (not Chromium).
    """
    consent_file = '/home/chronos/Consent To Send Stats'
    def _HasValidConsentFile():
      return os.path.isfile(consent_file) and len(open(consent_file).read())
    if not _HasValidConsentFile():
      client_id = hashlib.md5('abcdefgh').hexdigest()
      open(consent_file, 'w').write(client_id)
    assert _HasValidConsentFile(), 'Could not create %s' % consent_file

  def ExtraChromeFlagsOnChromeOS(self):
    """Return a list of extra chrome flags to use with chrome for testing.

    These are flags needed to facilitate testing.
    """
    return [
       '--homepage=about:blank',
       '--allow-file-access',
       '--allow-file-access-from-files',
       '--enable-file-cookies',
       '--dom-automation',
    ]

  @staticmethod
  def _IsSessionManagerReady(old_pid):
    """Is the ChromeOS session_manager running and ready to accept DBus calls?

    Called after session_manager is killed to know when it has restarted.

    Args:
      old_pid: The pid that session_manager had before it was killed,
               to ensure that we don't look at the DBus interface
               of an old session_manager process.
    """
    pgrep_process = subprocess.Popen(['pgrep', 'session_manager'],
                                     stdout=subprocess.PIPE)
    new_pid = pgrep_process.communicate()[0].strip()
    if not new_pid or old_pid == new_pid:
      return False

    import dbus
    try:
      bus = dbus.SystemBus()
      proxy = bus.get_object('org.chromium.SessionManager',
                             '/org/chromium/SessionManager')
      dbus.Interface(proxy, 'org.chromium.SessionManagerInterface')
    except dbus.DBusException:
      return False
    return True

  @staticmethod
  def CleanupBrowserProfileOnChromeOS():
    """Cleanup browser profile dir on ChromeOS.

    Browser should not be running, or else there will be locked files.
    """
    profile_dir = '/home/chronos/user'
    for item in os.listdir(profile_dir):
      # Deleting .pki causes stateful partition to get erased
      if item != '.pki' and item != 'log':
        pyauto_utils.RemovePath(os.path.join(profile_dir, item))

    chronos_dir = '/home/chronos'
    for item in os.listdir(chronos_dir):
      if item != 'user' and not item.startswith('.'):
        pyauto_utils.RemovePath(os.path.join(chronos_dir, item))

  @staticmethod
  def _IsInodeNew(path, old_inode):
    """Determine whether an inode has changed. POSIX only.

    Args:
      path: The file path to check for changes.
      old_inode: The old inode number.

    Returns:
      True if the path exists and its inode number is different from old_inode.
      False otherwise.
    """
    try:
      stat_result = os.stat(path)
    except OSError:
      return False
    if not stat_result:
      return False
    return stat_result.st_ino != old_inode

  def RestartBrowser(self, clear_profile=True):
    """Restart the browser.

    For use with tests that require to restart the browser.

    Args:
      clear_profile: If True, the browser profile is cleared before restart.
                     Defaults to True, that is restarts browser with a clean
                     profile.
    """
    if self.IsChromeOS():
      self.TearDown()
      if clear_profile:
        self.CleanupBrowserProfileOnChromeOS()
      self.CloseChromeOnChromeOS()
      self.EnableChromeTestingOnChromeOS()
      self.SetUp()
      return
    # Not chromeos
    orig_clear_state = self.get_clear_profile()
    self.CloseBrowserAndServer()
    self.set_clear_profile(clear_profile)
    logging.debug('Restarting browser with clear_profile=%s' %
                  self.get_clear_profile())
    self.LaunchBrowserAndServer()
    self.set_clear_profile(orig_clear_state)  # Reset to original state.

  @staticmethod
  def DataDir():
    """Returns the path to the data dir chrome/test/data."""
    return os.path.normpath(
        os.path.join(os.path.dirname(__file__), os.pardir, "data"))

  @staticmethod
  def GetFileURLForPath(*path):
    """Get file:// url for the given path.

    Also quotes the url using urllib.quote().

    Args:
      path: Variable number of strings that can be joined.
    """
    path_str = os.path.join(*path)
    abs_path = os.path.abspath(path_str)
    if sys.platform == 'win32':
      # Don't quote the ':' in drive letter ( say, C: ) on win.
      # Also, replace '\' with '/' as expected in a file:/// url.
      drive, rest = os.path.splitdrive(abs_path)
      quoted_path = drive.upper() + urllib.quote((rest.replace('\\', '/')))
      return 'file:///' + quoted_path
    else:
      quoted_path = urllib.quote(abs_path)
      return 'file://' + quoted_path

  @staticmethod
  def GetFileURLForDataPath(*relative_path):
    """Get file:// url for the given path relative to the chrome test data dir.

    Also quotes the url using urllib.quote().

    Args:
      relative_path: Variable number of strings that can be joined.
    """
    return PyUITest.GetFileURLForPath(PyUITest.DataDir(), *relative_path)

  @staticmethod
  def GetHttpURLForDataPath(*relative_path):
    """Get http:// url for the given path in the data dir.

    The URL will be usable only after starting the http server.
    """
    global _HTTP_SERVER
    assert _HTTP_SERVER, 'HTTP Server not yet started'
    return _HTTP_SERVER.GetURL(os.path.join('files', *relative_path)).spec()

  @staticmethod
  def IsMac():
    """Are we on Mac?"""
    return 'darwin' == sys.platform

  @staticmethod
  def IsLinux():
    """Are we on Linux? ChromeOS is linux too."""
    return 'linux2' == sys.platform

  @staticmethod
  def IsWin():
    """Are we on Win?"""
    return 'win32' == sys.platform

  @staticmethod
  def IsChromeOS():
    """Are we on ChromeOS (or Chromium OS)?

    Checks for "CHROMEOS_RELEASE_NAME=" in /etc/lsb-release.
    """
    lsb_release = '/etc/lsb-release'
    if not PyUITest.IsLinux() or not os.path.isfile(lsb_release):
      return False
    for line in open(lsb_release).readlines():
      if line.startswith('CHROMEOS_RELEASE_NAME='):
        return True
    return False

  @staticmethod
  def IsPosix():
    """Are we on Mac/Linux?"""
    return PyUITest.IsMac() or PyUITest.IsLinux()

  @staticmethod
  def IsEnUS():
    """Are we en-US?"""
    # TODO: figure out the machine's langugage.
    return True

  @staticmethod
  def EvalDataFrom(filename):
    """Return eval of python code from given file.

    The datastructure used in the file will be preserved.
    """
    data_file = os.path.join(filename)
    contents = open(data_file).read()
    try:
      ret = eval(contents, {'__builtins__': None}, None)
    except:
      print >>sys.stderr, '%s is an invalid data file.' % data_file
      raise
    return ret

  @staticmethod
  def ChromeOSBoard():
    """What is the ChromeOS board name"""
    if PyUITest.IsChromeOS():
      for line in open('/etc/lsb-release'):
        line = line.strip()
        if line.startswith('CHROMEOS_RELEASE_BOARD='):
          return line.split('=')[1]
    return None

  @staticmethod
  def Kill(pid):
    """Terminate the given pid.

    If the pid refers to a renderer, use KillRendererProcess instead.
    """
    if PyUITest.IsWin():
      subprocess.call(['taskkill.exe', '/T', '/F', '/PID', str(pid)])
    else:
      os.kill(pid, signal.SIGTERM)

  def GetPrivateInfo(self):
    """Fetch info from private_tests_info.txt in private dir.

    Returns:
      a dictionary of items from private_tests_info.txt
    """
    private_file = os.path.join(
        self.DataDir(), 'pyauto_private', 'private_tests_info.txt')
    assert os.path.exists(private_file), '%s missing' % private_file
    return self.EvalDataFrom(private_file)

  def WaitUntil(self, function, timeout=-1, retry_sleep=0.25, args=[],
                expect_retval=None):
    """Poll on a condition until timeout.

    Waits until the |function| evalues to |expect_retval| or until |timeout|
    secs, whichever occurs earlier.

    This is better than using a sleep, since it waits (almost) only as much
    as needed.

    WARNING: This method call should be avoided as far as possible in favor
    of a real wait from chromium (like wait-until-page-loaded).
    Only use in case there's really no better option.

    EXAMPLES:-
    Wait for "file.txt" to get created:
      WaitUntil(os.path.exists, args=["file.txt"])

    Same as above, but using lambda:
      WaitUntil(lambda: os.path.exists("file.txt"))

    Args:
      function: the function whose truth value is to be evaluated
      timeout: the max timeout (in secs) for which to wait. The default
               action is to wait for kWaitForActionMaxMsec, as set in
               ui_test.cc
               Use None to wait indefinitely.
      retry_sleep: the sleep interval (in secs) before retrying |function|.
                   Defaults to 0.25 secs.
      args: the args to pass to |function|
      expect_retval: the expected return value for |function|. This forms the
                     exit criteria. In case this is None (the default),
                     |function|'s return value is checked for truth,
                     so 'non-empty-string' should match with True

    Returns:
      True, if returning when |function| evaluated to True
      False, when returning due to timeout
    """
    if timeout == -1:  # Default
      timeout = self.action_max_timeout_ms() / 1000.0
    assert callable(function), "function should be a callable"
    begin = time.time()
    while timeout is None or time.time() - begin <= timeout:
      retval = function(*args)
      if (expect_retval is None and retval) or expect_retval == retval:
        return True
      logging.debug('WaitUntil(%s) still waiting. '
                    'Expecting %s. Last returned %s.' % (
                    function, expect_retval, retval))
      time.sleep(retry_sleep)
    return False


  class ActionTimeoutChanger(object):
    """Facilitate temporary changes to action_timeout_ms.

    Automatically resets to original timeout when object is destroyed.
    """
    _saved_timeout = -1  # Saved value for action_timeout_ms

    def __init__(self, ui_test, new_timeout):
      """Initialize.

      Args:
        ui_test: a PyUITest object
        new_timeout: new timeout to use (in milli secs)
      """
      self._saved_timeout = ui_test.action_timeout_ms()
      if new_timeout != self._saved_timeout:
        ui_test.set_action_timeout_ms(new_timeout)
      self._ui_test = ui_test

    def __del__(self):
      """Reset command_execution_timeout_ms to original value."""
      if self._ui_test.action_timeout_ms() != self._saved_timeout:
        self._ui_test.set_action_timeout_ms(self._saved_timeout)


  def _GetResultFromJSONRequest(self, cmd_dict, windex=0, timeout=-1):
    """Issue call over the JSON automation channel and fetch output.

    This method packages the given dictionary into a json string, sends it
    over the JSON automation channel, loads the json output string returned,
    and returns it back as a dictionary.

    Args:
      cmd_dict: the command dictionary. It must have a 'command' key
                Sample:
                  {
                    'command': 'SetOmniboxText',
                    'text': text,
                  }
      windex: 0-based window index on which to work. Default: 0 (first window)
              Use -ve windex if the automation command does not apply to a
              browser window. example: chromeos login

      timeout: request timeout (in milliseconds)

    Returns:
      a dictionary for the output returned by the automation channel.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    if timeout == -1:  # Default
      timeout = self.action_max_timeout_ms()
    result = self._SendJSONRequest(windex, json.dumps(cmd_dict), timeout)
    if len(result) == 0:
      raise JSONInterfaceError('Automation call received no response.')
    ret_dict = json.loads(result)
    if ret_dict.has_key('error'):
      raise JSONInterfaceError(ret_dict['error'])
    return ret_dict

  def GetBookmarkModel(self):
    """Return the bookmark model as a BookmarkModel object.

    This is a snapshot of the bookmark model; it is not a proxy and
    does not get updated as the bookmark model changes.
    """
    bookmarks_as_json = self._GetBookmarksAsJSON()
    if bookmarks_as_json == None:
      raise JSONInterfaceError('Could not resolve browser proxy.')
    return bookmark_model.BookmarkModel(bookmarks_as_json)

  def GetDownloadsInfo(self, windex=0):
    """Return info about downloads.

    This includes all the downloads recognized by the history system.

    Returns:
      an instance of downloads_info.DownloadInfo
    """
    return download_info.DownloadInfo(
        self._SendJSONRequest(
            windex, json.dumps({'command': 'GetDownloadsInfo'}),
            self.action_max_timeout_ms()))

  def GetOmniboxInfo(self, windex=0):
    """Return info about Omnibox.

    This represents a snapshot of the omnibox.  If you expect changes
    you need to call this method again to get a fresh snapshot.
    Note that this DOES NOT shift focus to the omnibox; you've to ensure that
    the omnibox is in focus or else you won't get any interesting info.

    It's OK to call this even when the omnibox popup is not showing.  In this
    case however, there won't be any matches, but other properties (like the
    current text in the omnibox) will still be fetched.

    Due to the nature of the omnibox, this function is sensitive to mouse
    focus.  DO NOT HOVER MOUSE OVER OMNIBOX OR CHANGE WINDOW FOCUS WHEN USING
    THIS METHOD.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)

    Returns:
      an instance of omnibox_info.OmniboxInfo
    """
    return omnibox_info.OmniboxInfo(
        self._SendJSONRequest(windex,
                              json.dumps({'command': 'GetOmniboxInfo'}),
                              self.action_max_timeout_ms()))

  def SetOmniboxText(self, text, windex=0):
    """Enter text into the omnibox. This shifts focus to the omnibox.

    Args:
      text: the text to be set.
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    # Ensure that keyword data is loaded from the profile.
    # This would normally be triggered by the user inputting this text.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {
        'command': 'SetOmniboxText',
        'text': text,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  # TODO(ace): Remove this hack, update bug 62783.
  def WaitUntilOmniboxReadyHack(self, windex=0):
    """Wait until the omnibox is ready for input.

    This is a hack workaround for linux platform, which returns from
    synchronous window creation methods before the omnibox is fully functional.

    No-op on non-linux platforms.

    Args:
      windex: the index of the browser to work on.
    """
    if self.IsLinux():
      return self.WaitUntil(
          lambda : self.GetOmniboxInfo(windex).Properties('has_focus'))

  def WaitUntilOmniboxQueryDone(self, windex=0):
    """Wait until omnibox has finished populating results.

    Uses WaitUntil() so the wait duration is capped by the timeout values
    used by automation, which WaitUntil() uses.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    return self.WaitUntil(
        lambda : not self.GetOmniboxInfo(windex).IsQueryInProgress())

  def OmniboxMovePopupSelection(self, count, windex=0):
    """Move omnibox popup selection up or down.

    Args:
      count: number of rows by which to move.
             -ve implies down, +ve implies up
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    cmd_dict = {
        'command': 'OmniboxMovePopupSelection',
        'count': count,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def OmniboxAcceptInput(self, windex=0):
    """Accepts the current string of text in the omnibox.

    This is equivalent to clicking or hiting enter on a popup selection.
    Blocks until the page loads.

    Args:
      windex: the index of the browser window to work on.
              Default: 0 (first window)
    """
    cmd_dict = {
        'command': 'OmniboxAcceptInput',
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def GetInstantInfo(self):
    """Return info about the instant overlay tab.

    Returns:
      A dictionary.
      Examples:
        { u'enabled': True,
          u'active': True,
          u'current': True,
          u'loading': True,
          u'location': u'http://cnn.com/',
          u'showing': False,
          u'title': u'CNN.com - Breaking News'},

        { u'enabled': False }
    """
    cmd_dict = {'command': 'GetInstantInfo'}
    return self._GetResultFromJSONRequest(cmd_dict)['instant']

  def GetSearchEngineInfo(self):
    """Return info about search engines.

    Returns:
      An ordered list of dictionaries describing info about each search engine.

      Example:
        [ { u'description': u'',
            u'display_url': u'{google:baseURL}search?q=%s',
            u'host': u'www.google.com',
            u'in_default_list': True,
            u'is_default': True,
            u'is_valid': True,
            u'keyword': u'google.com',
            u'path': u'/search',
            u'short_name': u'Google',
            u'supports_replacement': True,
            u'url': u'{google:baseURL}search?q={searchTerms}'},
          { u'description': u'',
            u'display_url': u'http://search.yahoo.com/search?p=%s',
            u'host': u'search.yahoo.com',
            u'in_default_list': True,
            u'is_default': False,
            u'is_valid': True,
            u'keyword': u'yahoo.com',
            u'path': u'/search',
            u'short_name': u'Yahoo!',
            u'supports_replacement': True,
            u'url': u'http://search.yahoo.com/search?p={searchTerms}'},
    """
    # Ensure that the search engine profile is loaded into data model.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {'command': 'GetSearchEngineInfo'}
    return self._GetResultFromJSONRequest(cmd_dict)['search_engines']

  def AddSearchEngine(self, title, keyword, url):
    """Add a search engine, as done through the search engines UI.

    Args:
      title: name for search engine.
      keyword: keyword, used to initiate a custom search from omnibox.
      url: url template for this search engine's query.
           '%s' is replaced by search query string when used to search.
    """
    # Ensure that the search engine profile is loaded into data model.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {'command': 'AddOrEditSearchEngine',
                'new_title': title,
                'new_keyword': keyword,
                'new_url': url}
    self._GetResultFromJSONRequest(cmd_dict)

  def EditSearchEngine(self, keyword, new_title, new_keyword, new_url):
    """Edit info for existing search engine.

    Args:
      keyword: existing search engine keyword.
      new_title: new name for this search engine.
      new_keyword: new keyword for this search engine.
      new_url: new url for this search engine.
    """
    # Ensure that the search engine profile is loaded into data model.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {'command': 'AddOrEditSearchEngine',
                'keyword': keyword,
                'new_title': new_title,
                'new_keyword': new_keyword,
                'new_url': new_url}
    self._GetResultFromJSONRequest(cmd_dict)

  def DeleteSearchEngine(self, keyword):
    """Delete search engine with given keyword.

    Args:
      keyword: the keyword string of the search engine to delete.
    """
    # Ensure that the search engine profile is loaded into data model.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {'command': 'PerformActionOnSearchEngine', 'keyword': keyword,
                'action': 'delete'}
    self._GetResultFromJSONRequest(cmd_dict)

  def MakeSearchEngineDefault(self, keyword):
    """Make search engine with given keyword the default search.

    Args:
      keyword: the keyword string of the search engine to make default.
    """
    # Ensure that the search engine profile is loaded into data model.
    self._GetResultFromJSONRequest({'command': 'LoadSearchEngineInfo'})
    cmd_dict = {'command': 'PerformActionOnSearchEngine', 'keyword': keyword,
                'action': 'default'}
    self._GetResultFromJSONRequest(cmd_dict)

  def GetPrefsInfo(self):
    """Return info about preferences.

    This represents a snapshot of the preferences. If you expect preferences
    to have changed, you need to call this method again to get a fresh
    snapshot.

    Returns:
      an instance of prefs_info.PrefsInfo
    """
    return prefs_info.PrefsInfo(
        self._SendJSONRequest(0, json.dumps({'command': 'GetPrefsInfo'}),
                              self.action_max_timeout_ms()))

  def SetPrefs(self, path, value):
    """Set preference for the given path.

    Preferences are stored by Chromium as a hierarchical dictionary.
    dot-separated paths can be used to refer to a particular preference.
    example: "session.restore_on_startup"

    Some preferences are managed, that is, they cannot be changed by the
    user. It's upto the user to know which ones can be changed. Typically,
    the options available via Chromium preferences can be changed.

    Args:
      path: the path the preference key that needs to be changed
            example: "session.restore_on_startup"
            One of the equivalent names in chrome/common/pref_names.h could
            also be used.
      value: the value to be set. It could be plain values like int, bool,
             string or complex ones like list.
             The user has to ensure that the right value is specified for the
             right key. It's useful to dump the preferences first to determine
             what type is expected for a particular preference path.
    """
    cmd_dict = {
      'command': 'SetPrefs',
      'path': path,
      'value': value,
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def SendWebkitKeyEvent(self, key_type, key_code, tab_index=0, windex=0):
    """Send a webkit key event to the browser.

    Args:
      key_type: the raw key type such as 0 for up and 3 for down.
      key_code: the hex value associated with the keypress (virtual key code).
      tab_index: tab index to work on. Defaults to 0 (first tab).
      windex: window index to work on. Defaults to 0 (first window).
    """
    cmd_dict = {
      'command': 'SendWebkitKeyEvent',
      'type': key_type,
      'text': '',
      'isSystemKey': False,
      'unmodifiedText': '',
      'nativeKeyCode': 0,
      'windowsKeyCode': key_code,
      'modifiers': 0,
      'windex': windex,
      'tab_index': tab_index,
    }
    # Sending request for key event.
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SendWebkitCharEvent(self, char, tab_index=0, windex=0):
    """Send a webkit char to the browser.

    Args:
      char: the char value to be sent to the browser.
      tab_index: tab index to work on. Defaults to 0 (first tab).
      windex: window index to work on. Defaults to 0 (first window).
    """
    cmd_dict = {
      'command': 'SendWebkitKeyEvent',
      'type': 2,  # kCharType
      'text': char,
      'isSystemKey': False,
      'unmodifiedText': char,
      'nativeKeyCode': 0,
      'windowsKeyCode': ord((char).upper()),
      'modifiers': 0,
      'windex': windex,
      'tab_index': tab_index,
    }
    # Sending request for a char.
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def WaitForAllDownloadsToComplete(self, windex=0, timeout=-1):
    """Wait for all downloads to complete.

    Note: This method does not work for dangerous downloads. Use
    WaitForGivenDownloadsToComplete (below) instead.
    """
    cmd_dict = {'command': 'WaitForAllDownloadsToComplete'}
    self._GetResultFromJSONRequest(cmd_dict, windex=windex, timeout=timeout)

  def WaitForDownloadToComplete(self, download_path, timeout=-1):
    """Wait for the given downloads to complete.

    This method works for dangerous downloads as well as regular downloads.

    Args:
      download_path: The path to the final download. This is only necessary for
                     the workaround described in the comments below and should
                     be removed when downloads are re-implemented.
      timeout: The timeout to use - default is WaitUntil's default timeout.
    """
    # TODO(alyssad): Remove this wait when downloads are re-implemented in a
    # testable way.
    self.WaitUntil(lambda path: os.path.exists(path), timeout=timeout,
                   args=[download_path])

  def PerformActionOnDownload(self, id, action, window_index=0):
    """Perform the given action on the download with the given id.

    Args:
      id: The id of the download.
      action: The action to perform on the download.
              Possible actions:
                'open': Opens the download (waits until it has completed first).
                'toggle_open_files_like_this': Toggles the 'Always Open Files
                    Of This Type' option.
                'remove': Removes the file from downloads (not from disk).
                'decline_dangerous_download': Equivalent to 'Discard' option
                    after downloading a dangerous download (ex. an executable).
                'save_dangerous_download': Equivalent to 'Save' option after
                    downloading a dangerous file.
                'toggle_pause': Toggles the paused state of the download. If the
                    download completed before this call, it's a no-op.
                'cancel': Cancel the download.
      window_index: The window index, default is 0.

    Returns:
      A dictionary representing the updated download item (except in the case
      of 'decline_dangerous_download', 'toggle_open_files_like_this', and
      'remove', which return an empty dict).
      Example dictionary:
      { u'PercentComplete': 100,
        u'file_name': u'file.txt',
        u'full_path': u'/path/to/file.txt',
        u'id': 0,
        u'is_extension_install': False,
        u'is_otr': False,
        u'is_paused': False,
        u'is_temporary': False,
        u'open_when_complete': False,
        u'referrer_url': u'',
        u'safety_state': u'SAFE',
        u'state': u'COMPLETE',
        u'url':  u'file://url/to/file.txt'
      }
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'PerformActionOnDownload',
      'id': id,
      'action': action
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def DownloadAndWaitForStart(self, file_url, windex=0):
    """Trigger download for the given url and wait for downloads to start.

    It waits for download by looking at the download info from Chrome, so
    anything which isn't registered by the history service won't be noticed.
    This is not thread-safe, but it's fine to call this method to start
    downloading multiple files in parallel. That is after starting a
    download, it's fine to start another one even if the first one hasn't
    completed.
    """
    try:
      num_downloads = len(self.GetDownloadsInfo(windex).Downloads())
    except JSONInterfaceError:
      num_downloads = 0

    self.NavigateToURL(file_url, windex)  # Trigger download.
    # It might take a while for the download to kick in, hold on until then.
    self.assertTrue(self.WaitUntil(
        lambda: len(self.GetDownloadsInfo(windex).Downloads()) >
                num_downloads))

  def SetWindowDimensions(
      self, x=None, y=None, width=None, height=None, windex=0):
    """Set window dimensions.

    All args are optional and current values will be preserved.
    Arbitrarily large values will be handled gracefully by the browser.

    Args:
      x: window origin x
      y: window origin y
      width: window width
      height: window height
      windex: window index to work on. Defaults to 0 (first window)
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SetWindowDimensions',
    }
    if x:
      cmd_dict['x'] = x
    if y:
      cmd_dict['y'] = y
    if width:
      cmd_dict['width'] = width
    if height:
      cmd_dict['height'] = height
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def WaitForInfobarCount(self, count, windex=0, tab_index=0):
    """Wait until infobar count becomes |count|.

    Note: Wait duration is capped by the automation timeout.

    Args:
      count: requested number of infobars
      windex: window index.  Defaults to 0 (first window)
      tab_index: tab index  Defaults to 0 (first tab)

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # TODO(phajdan.jr): We need a solid automation infrastructure to handle
    # these cases. See crbug.com/53647.
    def _InfobarCount():
      windows = self.GetBrowserInfo()['windows']
      if windex >= len(windows):  # not enough windows
        return -1
      tabs = windows[windex]['tabs']
      if tab_index >= len(tabs):  # not enough tabs
        return -1
      return len(tabs[tab_index]['infobars'])

    return self.WaitUntil(_InfobarCount, expect_retval=count)

  def PerformActionOnInfobar(
      self, action, infobar_index, windex=0, tab_index=0):
    """Perform actions on an infobar.

    Args:
      action: the action to be performed.
              Actions depend on the type of the infobar.  The user needs to
              call the right action for the right infobar.
              Valid inputs are:
              - "dismiss": closes the infobar (for all infobars)
              - "accept", "cancel": click accept / cancel (for confirm infobars)
      infobar_index: 0-based index of the infobar on which to perform the action
      windex: 0-based window index  Defaults to 0 (first window)
      tab_index: 0-based tab index.  Defaults to 0 (first tab)

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'PerformActionOnInfobar',
      'action': action,
      'infobar_index': infobar_index,
      'tab_index': tab_index,
    }
    if action not in ('dismiss', 'accept', 'cancel'):
      raise JSONInterfaceError('Invalid action %s' % action)
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def GetBrowserInfo(self):
    """Return info about the browser.

    This includes things like the version number, the executable name,
    executable path, pid info about the renderer/plugin/extension processes,
    window dimensions. (See sample below)

    For notification pid info, see 'GetActiveNotifications'.

    Returns:
      a dictionary

      Sample:
      { u'browser_pid': 93737,
        # Child processes are the processes for plugins and other workers.
        u'child_process_path': u'.../Chromium.app/Contents/'
                                'Versions/6.0.412.0/Chromium Helper.app/'
                                'Contents/MacOS/Chromium Helper',
        u'child_processes': [ { u'name': u'Shockwave Flash',
                                u'pid': 93766,
                                u'type': u'Plug-in'}],
        # There's one extension process per extension.
        u'extension_processes': [
          { u'name': u'Webpage Screenshot', u'pid': 93938},
          { u'name': u'Google Voice (by Google)', u'pid': 93852}],
        u'properties': {
          u'BrowserProcessExecutableName': u'Chromium',
          u'BrowserProcessExecutablePath': u'Chromium.app/Contents/MacOS/'
                                            'Chromium',
          u'ChromeVersion': u'6.0.412.0',
          u'HelperProcessExecutableName': u'Chromium Helper',
          u'HelperProcessExecutablePath': u'Chromium Helper.app/Contents/'
                                            'MacOS/Chromium Helper',
          u'command_line_string': "COMMAND_LINE_STRING --WITH-FLAGS",
          u'branding': 'Chromium',}
        # The order of the windows and tabs listed here will be the same as
        # what shows up on screen.
        u'windows': [ { u'index': 0,
                        u'height': 1134,
                        u'incognito': False,
                        u'is_fullscreen': False,
                        u'selected_tab': 0,
                        u'tabs': [ {
                          u'index': 0,
                          u'infobars': [],
                          u'pinned': True,
                          u'renderer_pid': 93747,
                          u'url': u'http://www.google.com/' }, {
                          u'index': 1,
                          u'infobars': [],
                          u'pinned': False,
                          u'renderer_pid': 93919,
                          u'url': u'https://chrome.google.com/'}, {
                          u'index': 2,
                          u'infobars': [ {
                            u'buttons': [u'Allow', u'Deny'],
                            u'link_text': u'Learn more',
                            u'text': u'slides.html5rocks.com wants to track '
                                      'your physical location',
                            u'type': u'confirm_infobar'}],
                          u'pinned': False,
                          u'renderer_pid': 93929,
                          u'url': u'http://slides.html5rocks.com/#slide14'},
                            ],
                        u'width': 925,
                        u'x': 26,
                        u'y': 44}]}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetBrowserInfo',
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def GetNavigationInfo(self, tab_index=0, windex=0):
    """Get info about the navigation state of a given tab.

    Args:
      tab_index: The tab index, default is 0.
      window_index: The window index, default is 0.

    Returns:
      a dictionary.
      Sample:

      { u'favicon_url': u'https://www.google.com/favicon.ico',
        u'page_type': u'NORMAL_PAGE',
        u'ssl': { u'displayed_insecure_content': False,
                  u'ran_insecure_content': False,
                  u'security_style': u'SECURITY_STYLE_AUTHENTICATED'}}

      Values for security_style can be:
        SECURITY_STYLE_UNKNOWN
        SECURITY_STYLE_UNAUTHENTICATED
        SECURITY_STYLE_AUTHENTICATION_BROKEN
        SECURITY_STYLE_AUTHENTICATED

      Values for page_type can be:
        NORMAL_PAGE
        ERROR_PAGE
        INTERSTITIAL_PAGE
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetNavigationInfo',
      'tab_index': tab_index,
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def GetHistoryInfo(self, search_text=''):
    """Return info about browsing history.

    Args:
      search_text: the string to search in history.  Defaults to empty string
                   which means that all history would be returned. This is
                   functionally equivalent to searching for a text in the
                   chrome://history UI. So partial matches work too.
                   When non-empty, the history items returned will contain a
                   "snippet" field corresponding to the snippet visible in
                   the chrome://history/ UI.

    Returns:
      an instance of history_info.HistoryInfo
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetHistoryInfo',
      'search_text': search_text,
    }
    return history_info.HistoryInfo(
        self._SendJSONRequest(0, json.dumps(cmd_dict),
                              self.action_max_timeout_ms()))

  def GetTranslateInfo(self, tab_index=0, window_index=0):
    """Returns info about translate for the given page.

    If the translate bar is showing, also returns information about the bar.

    Args:
      tab_index: The tab index, default is 0.
      window_index: The window index, default is 0.

    Returns:
      A dictionary of information about translate for the page. Example:
      { u'always_translate_lang_button_showing': False,
        u'never_translate_lang_button_showing': False,
        u'can_translate_page': True,
        u'original_language': u'es',
        u'page_translated': False,
        # The below will only appear if the translate bar is showing.
        u'translate_bar': { u'bar_state': u'BEFORE_TRANSLATE',
                            u'original_lang_code': u'es',
                            u'target_lang_code': u'en'}}
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetTranslateInfo',
      'tab_index': tab_index
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def ClickTranslateBarTranslate(self, tab_index=0, window_index=0):
    """If the translate bar is showing, clicks the 'Translate' button on the
       bar. This will show the 'this page has been translated...' infobar.

    Args:
      tab_index: The index of the tab, default is 0.
      window_index: The index of the window, default is 0.

    Returns:
      True if the translation was successful or false if there was an error.
      Note that an error shouldn't neccessarily mean a failed test - retry the
      call on error.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SelectTranslateOption',
      'tab_index': tab_index,
      'option': 'translate_page'
    }
    return self._GetResultFromJSONRequest(
        cmd_dict, windex=window_index)['translation_success']

  def RevertPageTranslation(self, tab_index=0, window_index=0):
    """Select the 'Show original' button on  the 'this page has been
       translated...' infobar. This will remove the infobar and revert the
       page translation.

    Args:
      tab_index: The index of the tab, default is 0.
      window_index: The index of the window, default is 0.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SelectTranslateOption',
      'tab_index': tab_index,
      'option': 'revert_translation'
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def ChangeTranslateToLanguage(self, new_language, tab_index=0,
                                window_index=0):
    """Set the target language to be a new language.

    This is equivalent to selecting a different language from the 'to'
    drop-down menu on the translate bar. If the page was already translated
    before calling this function, this will trigger a re-translate to the
    new language.

    Args:
      new_language: The new target language. The string should be equivalent
                    to the text seen in the translate bar options.
                    Example: 'English'.
      tab_index: The tab index - default is 0.
      window_index: The window index - default is 0.

    Returns:
      False, if a new translation was triggered and the translation failed.
      True on success.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SelectTranslateOption',
      'tab_index': tab_index,
      'option': 'set_target_language',
      'target_language': new_language
    }
    return self._GetResultFromJSONRequest(
        cmd_dict, windex=window_index)['translation_success']

  def GetExtensionsInfo(self):
    """Returns information about all installed extensions.

    Returns:
      A list of dictionaries representing each of the installed extensions.
      Example:
      [ { u'api_permissions': [u'bookmarks', u'experimental', u'tabs'],
          u'background_url': u'',
          u'description': u'Bookmark Manager',
          u'host_permissions': [u'chrome://favicon/*', u'chrome://resources/*'],
          u'id': u'eemcgdkfndhakfknompkggombfjjjeno',
          u'name': u'Bookmark Manager',
          u'options_url': u'',
          u'public_key': u'MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDQcByy+eN9jza\
                           zWF/DPn7NW47sW7lgmpk6eKc0BQM18q8hvEM3zNm2n7HkJv/R6f\
                           U+X5mtqkDuKvq5skF6qqUF4oEyaleWDFhd1xFwV7JV+/DU7bZ00\
                           w2+6gzqsabkerFpoP33ZRIw7OviJenP0c0uWqDWF8EGSyMhB3tx\
                           qhOtiQIDAQAB',
          u'version': u'0.1' },
        { u'api_permissions': [...],
          u'background_url': u'chrome-extension://\
                               lkdedmbpkaiahjjibfdmpoefffnbdkli/\
                               background.html',
          u'description': u'Extension which lets you read your Facebook news \
                            feed and wall. You can also post status updates.',
          u'host_permissions': [...],
          u'id': u'lkdedmbpkaiahjjibfdmpoefffnbdkli',
          u'name': u'Facebook for Google Chrome',
          u'options_url': u'',
          u'public_key': u'...',
          u'version': u'2.0.9' } ]
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetExtensionsInfo'
    }
    return self._GetResultFromJSONRequest(cmd_dict)['extensions']

  def UninstallExtensionById(self, id):
    """Uninstall the extension with the given id.

    Args:
      id: The string id of the extension. It can be retrieved through the
          GetExtensionsInfo call above.

    Returns:
      True, if the extension was successfully uninstalled, or
      False, otherwise.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'UninstallExtensionById',
      'id': id,
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def UpdateExtensionsNow(self):
    """Auto-updates installed extensions.

    Waits until all extensions are updated, loaded, and ready for use.
    This is equivalent to clicking the "Update extensions now" button on the
    chrome://extensions page.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface.
      'command': 'UpdateExtensionsNow',
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SelectTranslateOption(self, option, tab_index=0, window_index=0):
    """Selects one of the options in the drop-down menu for the translate bar.

    Args:
      option: One of 'never_translate_language', 'never_translate_site', or
              'toggle_always_translate'. See notes on each below.
      tab_index: The index of the tab, default is 0.
      window_index: The index of the window, default is 0.

    *Notes*
    never_translate_language: Selecting this means that no sites in this
      language will be translated. This dismisses the infobar.
    never_translate_site: Selecting this means that this site will never be
      translated, regardless of the language. This dismisses the infobar.
    toggle_always_translate: This does not dismiss the infobar or translate the
      page. See ClickTranslateBarTranslate and PerformActioOnInfobar to do
      those. If a language is selected to be always translated, then whenver
      the user visits a page with that language, the infobar will show the
      'This page has been translated...' message.
    decline_translation: Equivalent to selecting 'Nope' on the translate bar.
    click_never_translate_lang_button: This button appears when the user has
      declined translation of this language several times. Selecting it causes
      the language to never be translated. Look at GetTranslateInfo to
      determine if the button is showing.
    click_always_translate_lang_button: This button appears when the user has
      accepted translation of this language several times. Selecting it causes
      the language to always be translated. Look at GetTranslateInfo to
      determine if the button is showing.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SelectTranslateOption',
      'option': option,
      'tab_index': tab_index
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def WaitUntilTranslateComplete(self, tab_index=0, window_index=0):
    """Waits until an attempted translation has finished.

    This should be called after navigating to a page that should be translated
    automatically (because the language always-translate is on). It does not
    need to be called after 'ClickTranslateBarTranslate'.

    Do not call this function if you are not expecting a page translation - it
    will hang. If you call it when there is no translate bar, it will return
    False.

    Args:
      tab_index: The tab index, default is 0.
      window_index: The window index, default is 0.

    Returns:
      True if the translation was successful, False if there was an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'WaitUntilTranslateComplete',
      'tab_index': tab_index
    }
    # TODO(phajdan.jr): We need a solid automation infrastructure to handle
    # these cases. See crbug.com/53647.
    return self.WaitUntil(
        lambda tab_index, window_index: self.GetTranslateInfo(
            tab_index=tab_index, window_index=window_index)['page_translated'],
        args=[tab_index, window_index])

  def FillAutofillProfile(self, profiles=None, credit_cards=None,
                          tab_index=0, window_index=0):
    """Set the autofill profile to contain the given profiles and credit cards.

       If profiles or credit_cards are specified, they will overwrite existing
       profiles and credit cards. To update profiles and credit cards, get the
       existing ones with the GetAutofillProfile function and then append new
       profiles to the list and call this function.

    Args:
      profiles: (optional) a list of dictionaries representing each profile to
      add. Example:
      [{
        'NAME_FIRST': 'Bob',
        'NAME_LAST': 'Smith',
        'ADDRESS_HOME_ZIP': '94043',
      },
      {
        'EMAIL_ADDRESS': 'sue@example.com',
        'COMPANY_NAME': 'Company X',
      }]

      Other possible keys are:
      'NAME_FIRST', 'NAME_MIDDLE', 'NAME_LAST', 'EMAIL_ADDRESS',
      'COMPANY_NAME', 'ADDRESS_HOME_LINE1', 'ADDRESS_HOME_LINE2',
      'ADDRESS_HOME_CITY', 'ADDRESS_HOME_STATE', 'ADDRESS_HOME_ZIP',
      'ADDRESS_HOME_COUNTRY', 'PHONE_HOME_WHOLE_NUMBER',
      'PHONE_FAX_WHOLE_NUMBER'

      All values must be strings.

      credit_cards: (optional) a list of dictionaries representing each credit
      card to add. Example:
      [{
        'CREDIT_CARD_NAME': 'Bob C. Smith',
        'CREDIT_CARD_NUMBER': '5555555555554444',
        'CREDIT_CARD_EXP_MONTH': '12',
        'CREDIT_CARD_EXP_4_DIGIT_YEAR': '2011'
      },
      {
        'CREDIT_CARD_NAME': 'Bob C. Smith',
        'CREDIT_CARD_NUMBER': '4111111111111111',
        'CREDIT_CARD_TYPE': 'Visa'
      }

      Other possible keys are:
      'CREDIT_CARD_NAME', 'CREDIT_CARD_NUMBER', 'CREDIT_CARD_EXP_MONTH',
      'CREDIT_CARD_EXP_4_DIGIT_YEAR'

      All values must be strings.

      tab_index: tab index, defaults to 0.

      window_index: window index, defaults to 0.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'FillAutofillProfile',
      'tab_index': tab_index,
      'profiles': profiles,
      'credit_cards': credit_cards
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def GetAutofillProfile(self, tab_index=0, window_index=0):
    """Return the profile including all profiles and credit cards currently
       saved as a list of dictionaries.

       The format of the returned dictionary is described above in
       FillAutofillProfile. The general format is:
       {'profiles': [list of profile dictionaries as described above],
        'credit_cards': [list of credit card dictionaries as described above]}

    Args:
       tab_index: tab index, defaults to 0.
       window_index: window index, defaults to 0.

    Raises:
       pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetAutofillProfile',
      'tab_index': tab_index
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def AutofillTriggerSuggestions(self, field_id=None, tab_index=0, windex=0):
    """Focuses a webpage form field and triggers the autofill popup in it.

    This function focuses the specified input field in a webpage form, then
    causes the autofill popup to appear in that field.  The underlying
    automation hook sends a "down arrow" keypress event to trigger the autofill
    popup.  This function waits until the popup is displayed before returning.

    Args:
      field_id: The string ID of the webpage form field to focus.  Can be
                'None' (the default), in which case nothing is focused.  This
                can be useful if the field has already been focused by other
                means.
      tab_index: Integer index of the tab to work on; defaults to 0 (first tab).
      windex: Integer index of the browser window to work on; defaults to 0
              (first window).

    Returns:
      True, if no errors were encountered, or False otherwise.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # Focus the field with the specified ID, if necessary.
    if field_id:
      if not self.JavascriptFocusElementById(field_id, tab_index, windex):
        return False

    # Cause the autofill popup to be shown in the focused form field.
    cmd_dict = {
      'command': 'AutofillTriggerSuggestions',
      'tab_index': tab_index,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)
    return True

  def AutofillHighlightSuggestion(self, direction, tab_index=0, windex=0):
    """Highlights the previous or next suggestion in an existing autofill popup.

    This function assumes that an existing autofill popup is currently displayed
    in a webpage form.  The underlying automation hook sends either a
    "down arrow" or an "up arrow" keypress event to cause the next or previous
    suggestion to be highlighted, respectively.  This function waits until
    autofill displays a preview of the form's filled state before returning.

    Use AutofillTriggerSuggestions() to trigger the autofill popup before
    calling this function.  Use AutofillAcceptSelection() after calling this
    function to accept a selection.

    Args:
      direction: The string direction in which to highlight an autofill
                 suggestion.  Must be either "up" or "down".
      tab_index: Integer index of the tab to work on; defaults to 0 (first tab).
      windex: Integer index of the browser window to work on; defaults to 0
              (first window).

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    assert direction in ('up', 'down')
    cmd_dict = {
      'command': 'AutofillHighlightSuggestion',
      'direction': direction,
      'tab_index': tab_index,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def AutofillAcceptSelection(self, tab_index=0, windex=0):
    """Accepts the current selection in an already-displayed autofill popup.

    This function assumes that a profile is already highlighted in an existing
    autofill popup in a webpage form.  The underlying automation hook sends a
    "return" keypress event to cause the highlighted profile to be accepted.
    This function waits for the webpage form to be filled in with autofill data
    before returning.  This function does not submit the webpage form.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'AutofillAcceptSelection',
      'tab_index': tab_index,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def AutofillPopulateForm(self, field_id, profile_index=0, tab_index=0,
                           windex=0):
    """Populates a webpage form using autofill data and keypress events.

    This function focuses the specified input field in the form, and then
    sends keypress events to the associated tab to cause the form to be
    populated with information from the requested autofill profile.

    Args:
      field_id: The string ID of the webpage form field to focus for autofill
                purposes.
      profile_index: The index of the profile in the autofill popup to use to
                     populate the form; defaults to 0 (first profile).
      tab_index: Integer index of the tab to work on; defaults to 0 (first tab).
      windex: Integer index of the browser window to work on; defaults to 0
              (first window).

    Returns:
      True, if the webpage form is populated successfully, or False if not.

    Raises:
      pyauto_errors.JSONInterfaceError if an automation call returns an error.
    """
    if not self.AutofillTriggerSuggestions(field_id, tab_index, windex):
      return False

    for _ in range(profile_index + 1):
      self.AutofillHighlightSuggestion('down', tab_index, windex)

    self.AutofillAcceptSelection(tab_index, windex)
    return True

  def AddHistoryItem(self, item):
    """Forge a history item for Chrome.

    Args:
      item: a python dictionary representing the history item.  Example:
      {
        # URL is the only mandatory item.
        'url': 'http://news.google.com',
        # Title is optional.
        'title': 'Google News',
        # Time is optional; if not set, assume "now".  Time is in
        # seconds since the Epoch.  The python construct to get "Now"
        # in the right scale is "time.time()".  Can be float or int.
        'time': 1271781612
      }
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'AddHistoryItem',
      'item': item
    }
    if not 'url' in item:
      raise JSONInterfaceError('must specify url')
    self._GetResultFromJSONRequest(cmd_dict)

  def GetPluginsInfo(self):
    """Return info about plugins.

    This is the info available from about:plugins

    Returns:
      an instance of plugins_info.PluginsInfo
    """
    return plugins_info.PluginsInfo(
        self._SendJSONRequest(0, json.dumps({'command': 'GetPluginsInfo'}),
                              self.action_max_timeout_ms()))

  def EnablePlugin(self, path):
    """Enable the plugin at the given path.

    Use GetPluginsInfo() to fetch path info about a plugin.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'EnablePlugin',
      'path': path,
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def DisablePlugin(self, path):
    """Disable the plugin at the given path.

    Use GetPluginsInfo() to fetch path info about a plugin.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'DisablePlugin',
      'path': path,
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def GetTabContents(self, tab_index=0, window_index=0):
    """Get the html contents of a tab (a la "view source").

    As an implementation detail, this saves the html in a file, reads
    the file into a buffer, then deletes it.

    Args:
      tab_index: tab index, defaults to 0.
      window_index: window index, defaults to 0.
    Returns:
      html content of a page as a string.
    """
    tempdir = tempfile.mkdtemp()
    filename = os.path.join(tempdir, 'content.html')
    cmd_dict = {  # Prepare command for the json interface
      'command': 'SaveTabContents',
      'tab_index': tab_index,
      'filename': filename
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=window_index)
    try:
      f = open(filename)
      all_data = f.read()
      f.close()
      return all_data
    finally:
      shutil.rmtree(tempdir)

  def ImportSettings(self, import_from, first_run, import_items):
    """Import the specified import items from the specified browser.

    Implements the features available in the "Import Settings" part of the
    first-run UI dialog.

    Args:
      import_from: A string indicating which browser to import from. Possible
                   strings (depending on which browsers are installed on the
                   machine) are: 'Mozilla Firefox', 'Google Toolbar',
                   'Microsoft Internet Explorer', 'Safari'
      first_run: A boolean indicating whether this is the first run of
                 the browser.
                 If it is not the first run then:
                 1) Bookmarks are only imported to the bookmarks bar if there
                    aren't already bookmarks.
                 2) The bookmark bar is shown.
      import_items: A list of strings indicating which items to import.
                    Strings that can be in the list are:
                    HISTORY, FAVORITES, PASSWORDS, SEARCH_ENGINES, HOME_PAGE,
                    ALL (note: COOKIES is not supported by the browser yet)
    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'ImportSettings',
      'import_from': import_from,
      'first_run': first_run,
      'import_items': import_items
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def ClearBrowsingData(self, to_remove, time_period):
    """Clear the specified browsing data. Implements the features available in
       the "ClearBrowsingData" UI.

    Args:
      to_remove: a list of strings indicating which types of browsing data
                 should be removed. Strings that can be in the list are:
                 HISTORY, DOWNLOADS, COOKIES, PASSWORDS, FORM_DATA, CACHE
      time_period: a string indicating the time period for the removal.
                   Possible strings are:
                   LAST_HOUR, LAST_DAY, LAST_WEEK, FOUR_WEEKS, EVERYTHING

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'ClearBrowsingData',
      'to_remove': to_remove,
      'time_period': time_period
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def AddSavedPassword(self, password_dict, window_index=0):
    """Adds the given username-password combination to the saved passwords.

    Args:
      password_dict: a dictionary that represents a password. Example:
      { 'username_value': 'user@example.com',        # Required
        'password_value': 'test.password',           # Required
        'signon_realm': 'https://www.example.com/',  # Required
        'time': 1279317810.0,                        # Can get from time.time()
        'origin_url': 'https://www.example.com/login',
        'username_element': 'username',              # The HTML element
        'password_element': 'password',              # The HTML element
        'submit_element': 'submit',                  # The HTML element
        'action_target': 'https://www.example.com/login/',
        'blacklist': False }
      window_index: window index, defaults to 0

    *Blacklist notes* To blacklist a site, add a blacklist password with the
    following dictionary items: origin_url, signon_realm, username_element,
    password_element, action_target, and 'blacklist': True. Then all sites that
    have password forms matching those are blacklisted.

    Returns:
      True if adding the password succeeded, false otherwise. In incognito
      mode, adding the password should fail.

    Raises:
      JSONInterfaceError on error.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'AddSavedPassword',
      'password': password_dict
    }
    return self._GetResultFromJSONRequest(
        cmd_dict, windex=window_index)['password_added']

  def RemoveSavedPassword(self, password_dict, window_index=0):
    """Removes the password matching the provided password dictionary.

    Args:
      password_dict: A dictionary that represents a password.
                     For an example, see the dictionary in AddSavedPassword.
      window_index: The window index, default is 0.
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'RemoveSavedPassword',
      'password': password_dict
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=window_index)

  def GetSavedPasswords(self):
    """Return the passwords currently saved.

    Returns:
      A list of dictionaries representing each password. For an example
      dictionary see AddSavedPassword documentation. The overall structure will
      be:
      [ {password1 dictionary}, {password2 dictionary} ]
    """
    cmd_dict = {  # Prepare command for the json interface
      'command': 'GetSavedPasswords'
    }
    return self._GetResultFromJSONRequest(cmd_dict)['passwords']

  def GetBlockedPopupsInfo(self, tab_index=0, windex=0):
    """Get info about blocked popups in a tab.

    Args:
      tab_index: 0-based tab index. Default: 0
      windex: 0-based window index. Default: 0

    Returns:
      [a list of property dictionaries for each blocked popup]
      Property dictionary contains: title, url
    """
    cmd_dict = {
      'command': 'GetBlockedPopupsInfo',
      'tab_index': tab_index,
    }
    return self._GetResultFromJSONRequest(cmd_dict,
                                          windex=windex)['blocked_popups']

  def UnblockAndLaunchBlockedPopup(self, popup_index, tab_index=0, windex=0):
    """Unblock/launch a poup at the given index.

    This is equivalent to clicking on a blocked popup in the UI available
    from the omnibox.
    """
    cmd_dict = {
      'command': 'UnblockAndLaunchBlockedPopup',
      'popup_index': popup_index,
      'tab_index': tab_index,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def SetTheme(self, crx_file_path):
    """Installs the given theme synchronously.

    A theme file is a file with a .crx suffix, like an extension.  The theme
    file must be specified with an absolute path.  This method call waits until
    the theme is installed and will trigger the "theme installed" infobar.

    Uses InstallExtension().

    Returns:
      The ID of the installed theme, on success.  The empty string, otherwise.
    """
    return self.InstallExtension(crx_file_path, True)

  def WaitUntilDownloadedThemeSet(self, theme_name):
    """Waits until the theme has been set.

    This should not be called after SetTheme(). It only needs to be called after
    downloading a theme file (which will automatically set the theme).

    Uses WaitUntil so timeout is capped by automation timeout.

    Args:
      theme_name: The name that the theme will have once it is installed.
    """
    def _ReturnThemeSet(name):
      theme_info = self.GetThemeInfo()
      return theme_info and theme_info['name'] == name
    return self.WaitUntil(_ReturnThemeSet, args=[theme_name])

  def ClearTheme(self):
    """Clear the theme.  Resets to default.

    Has no effect when the theme is already the default one.
    This is a blocking call.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'ClearTheme',
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def GetThemeInfo(self):
    """Get info about theme.

    This includes info about the theme name, its colors, images, etc.

    Returns:
      a dictionary containing info about the theme.
      empty dictionary if no theme has been applied (default theme).
    SAMPLE:
    { u'colors': { u'frame': [71, 105, 91],
                   u'ntp_link': [36, 70, 0],
                   u'ntp_section': [207, 221, 192],
                   u'ntp_text': [20, 40, 0],
                   u'toolbar': [207, 221, 192]},
      u'images': { u'theme_frame': u'images/theme_frame_camo.png',
                   u'theme_ntp_background': u'images/theme_ntp_background.png',
                   u'theme_toolbar': u'images/theme_toolbar_camo.png'},
      u'name': u'camo theme',
      u'tints': {u'buttons': [0.33000000000000002, 0.5, 0.46999999999999997]}}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'GetThemeInfo',
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def GetActiveNotifications(self):
    """Gets a list of the currently active/shown HTML5 notifications.

    Returns:
      a list containing info about each active notification, with the
      first item in the list being the notification on the bottom of the
      notification stack. The 'content_url' key can refer to a URL or a data
      URI. The 'pid' key-value pair may be omitted or invalid if the
      notification is closing.

    SAMPLE:
    [ { u'content_url': u'data:text/html;charset=utf-8,%3C!DOCTYPE%l%3E%0Atm...'
        u'display_source': 'www.corp.google.com',
        u'origin_url': 'http://www.corp.google.com/',
        u'pid': 8505},
      { u'content_url': 'http://www.gmail.com/special_notification.html',
        u'display_source': 'www.gmail.com',
        u'origin_url': 'http://www.gmail.com/',
        u'pid': 9291}]

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'GetActiveNotifications',
    }
    return self._GetResultFromJSONRequest(cmd_dict)['notifications']

  def CloseNotification(self, index):
    """Closes the active HTML5 notification at the given index.

    Args:
      index: the index of the notification to close. 0 refers to the
             notification on the bottom of the notification stack.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'CloseNotification',
      'index': index,
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def WaitForNotificationCount(self, count):
    """Waits for the number of active HTML5 notifications to reach the given
    count.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'WaitForNotificationCount',
      'count': count,
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def FindInPage(self, search_string, forward=True,
                 match_case=False, find_next=False,
                 tab_index=0, windex=0):
    """Find the match count for the given search string and search parameters.
    This is equivalent to using the find box

    Args:
      search_string: The string to find on the page.
      forward: Boolean to set if the search direction is forward or backwards
      match_case: Boolean to set for case sensitive search.
      find_next: Boolean to set to continue the search or start from beginning.
      tab_index: The tab index, default is 0.
      window_index: The window index, default is 0.

    Returns:
      number of matches found for the given search string and parameters
    SAMPLE:
    { u'match_count': 10,
      u'match_left': 100,
      u'match_top': 100,
      u'match_right': 200,
      u'match_bottom': 200}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'FindInPage',
      'tab_index' : tab_index,
      'search_string' : search_string,
      'forward' : forward,
      'match_case' : match_case,
      'find_next' : find_next,
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def CallJavascriptFunc(self, function, args=[], tab_index=0, windex=0):
    """Executes a script which calls a given javascript function.

    The invoked javascript function must send a result back via the
    domAutomationController.send function, or this function will never return.

    Defaults to first tab in first window.

    Args:
      function: name of the function
      args: list of all the arguments to pass into the called function. These
            should be able to be converted to a string using the |str| function.
      tab_index: index of the tab within the given window
      windex: index of the window

    Returns:
      a string that was sent back via the domAutomationController.send method
    """
    converted_args = map(lambda arg: json.dumps(arg), args)
    js = '%s(%s)' % (function, ', '.join(converted_args))
    logging.debug('Executing javascript: %s', js)
    return self.ExecuteJavascript(js, windex, tab_index)

  def JavascriptFocusElementById(self, field_id, tab_index=0, windex=0):
    """Uses Javascript to focus an element with the given ID in a webpage.

    Args:
      field_id: The string ID of the webpage form field to focus.
      tab_index: Integer index of the tab to work on; defaults to 0 (first tab).
      windex: Integer index of the browser window to work on; defaults to 0
              (first window).

    Returns:
      True, on success, or False on failure.
    """
    focus_field_js = """
        var field = document.getElementById("%s");
        if (!field) {
          window.domAutomationController.send("error");
        } else {
          field.focus();
          window.domAutomationController.send("done");
        }
    """ % field_id
    return self.ExecuteJavascript(focus_field_js, windex, tab_index) == 'done'

  def SignInToSync(self, username, password):
    """Signs in to sync using the given username and password.

    Args:
      username: The account with which to sign in. Example: "user@gmail.com".
      password: Password for the above account. Example: "pa$$w0rd".

    Returns:
      True, on success.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'SignInToSync',
      'username': username,
      'password': password,
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def GetSyncInfo(self):
    """Returns info about sync.

    Returns:
      A dictionary of info about sync.
      Example dictionaries:
        {u'summary': u'SYNC DISABLED'}

        { u'authenticated': True,
          u'last synced': u'Just now',
          u'summary': u'READY',
          u'sync url': u'clients4.google.com',
          u'updates received': 42,
          u'synced datatypes': [ u'Bookmarks',
                                 u'Preferences',
                                 u'Passwords',
                                 u'Autofill',
                                 u'Themes',
                                 u'Extensions',
                                 u'Apps']}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'GetSyncInfo',
    }
    return self._GetResultFromJSONRequest(cmd_dict)['sync_info']

  def AwaitSyncCycleCompletion(self):
    """Waits for the ongoing sync cycle to complete. Must be signed in to sync
       before calling this method.

    Returns:
      True, on success.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'AwaitSyncCycleCompletion',
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def AwaitSyncRestart(self):
    """Waits for sync to reinitialize itself. Typically used when the browser
       is restarted and a full sync cycle is not expected to occur. Must be
       previously signed in to sync before calling this method.

    Returns:
      True, on success.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'AwaitSyncRestart',
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def EnableSyncForDatatypes(self, datatypes):
    """Enables sync for a given list of sync datatypes. Must be signed in to
       sync before calling this method.

    Args:
      datatypes: A list of strings indicating the datatypes for which to enable
                 sync. Strings that can be in the list are:
                 Bookmarks, Preferences, Passwords, Autofill, Themes,
                 Typed URLs, Extensions, Encryption keys, Sessions, Apps, All.
                 For an updated list of valid sync datatypes, refer to the
                 function ModelTypeToString() in the file
                 chrome/browser/sync/syncable/model_type.cc.
                 Examples:
                   ['Bookmarks', 'Preferences', 'Passwords']
                   ['All']

    Returns:
      True, on success.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'EnableSyncForDatatypes',
      'datatypes': datatypes,
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def DisableSyncForDatatypes(self, datatypes):
    """Disables sync for a given list of sync datatypes. Must be signed in to
       sync before calling this method.

    Args:
      datatypes: A list of strings indicating the datatypes for which to
                 disable sync. Strings that can be in the list are:
                 Bookmarks, Preferences, Passwords, Autofill, Themes,
                 Typed URLs, Extensions, Encryption keys, Sessions, Apps, All.
                 For an updated list of valid sync datatypes, refer to the
                 function ModelTypeToString() in the file
                 chrome/browser/sync/syncable/model_type.cc.
                 Examples:
                   ['Bookmarks', 'Preferences', 'Passwords']
                   ['All']

    Returns:
      True, on success.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'DisableSyncForDatatypes',
      'datatypes': datatypes,
    }
    return self._GetResultFromJSONRequest(cmd_dict)['success']

  def GetNTPThumbnails(self):
    """Return a list of info about the sites in the NTP most visited section.
    SAMPLE:
      [{ u'title': u'Google',
         u'url': u'http://www.google.com',
         u'is_pinned': False},
       {
         u'title': u'Yahoo',
         u'url': u'http://www.yahoo.com',
         u'is_pinned': True}]
    """
    return self._GetNTPInfo()['most_visited']

  def GetNTPThumbnailIndex(self, thumbnail):
    """Returns the index of the given NTP thumbnail, or -1 if it is not shown.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
    """
    thumbnails = self.GetNTPThumbnails()
    for i in range(len(thumbnails)):
      if thumbnails[i]['url'] == thumbnail['url']:
        return i
    return -1

  def MoveNTPThumbnail(self, thumbnail, new_index):
    """Moves the given thumbnail to a new index. The indices in the NTP Most
    Visited sites section look like:
      0  1  2  3
      4  5  6  7

    When a thumbnail is moved, it is automatically pinned.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
      new_index: the index to be moved to in the Most Visited sites section

    Raises:
      IndexError if there is no thumbnail at the index
    """
    if new_index < 0 or new_index >= len(self.GetNTPThumbnails()):
      raise IndexError()
    self._CheckNTPThumbnailShown(thumbnail)
    cmd_dict = {
      'command': 'MoveNTPMostVisitedThumbnail',
      'url': thumbnail['url'],
      'index': new_index,
      'old_index': self.GetNTPThumbnailIndex(thumbnail)
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def RemoveNTPThumbnail(self, thumbnail):
    """Removes the NTP thumbnail and returns true on success.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
    """
    self._CheckNTPThumbnailShown(thumbnail)
    cmd_dict = {
      'command': 'RemoveNTPMostVisitedThumbnail',
      'url': thumbnail['url']
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def PinNTPThumbnail(self, thumbnail):
    """Pins the NTP thumbnail.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
    """
    self._CheckNTPThumbnailShown(thumbnail)
    self.MoveNTPThumbnail(thumbnail, self.GetNTPThumbnailIndex(thumbnail))

  def UnpinNTPThumbnail(self, thumbnail):
    """Unpins the NTP thumbnail and returns true on success.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
    """
    self._CheckNTPThumbnailShown(thumbnail)
    cmd_dict = {
      'command': 'UnpinNTPMostVisitedThumbnail',
      'url': thumbnail['url']
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def IsNTPThumbnailPinned(self, thumbnail):
    """Returns whether the NTP thumbnail is pinned.

    Args:
      thumbnail: a thumbnail dict received from |GetNTPThumbnails|
    """
    self._CheckNTPThumbnailShown(thumbnail)
    index = self.GetNTPThumbnailIndex(thumbnail)
    return self.GetNTPThumbnails()[index]['is_pinned']

  def RestoreAllNTPThumbnails(self):
    """Restores all the removed NTP thumbnails.
    Note:
      the default thumbnails may come back into the Most Visited sites
      section after doing this
    """
    cmd_dict = {
      'command': 'RestoreAllNTPMostVisitedThumbnails'
    }
    self._GetResultFromJSONRequest(cmd_dict)

  def GetNTPDefaultSites(self):
    """Returns a list of URLs for all the default NTP sites, regardless of
    whether they are showing or not.

    These sites are the ones present in the NTP on a fresh install of Chrome.
    """
    return self._GetNTPInfo()['default_sites']

  def RemoveNTPDefaultThumbnails(self):
    """Removes all thumbnails for default NTP sites, regardless of whether they
    are showing or not."""
    cmd_dict = { 'command': 'RemoveNTPMostVisitedThumbnail' }
    for site in self.GetNTPDefaultSites():
      cmd_dict['url'] = site
      self._GetResultFromJSONRequest(cmd_dict)

  def GetNTPRecentlyClosed(self):
    """Return a list of info about the items in the NTP recently closed section.
    SAMPLE:
      [{
         u'type': u'tab',
         u'url': u'http://www.bing.com',
         u'title': u'Bing',
         u'timestamp': 2139082.03912,  # Seconds since epoch (Jan 1, 1970)
         u'direction': u'ltr'},
       {
         u'type': u'window',
         u'timestamp': 2130821.90812,
         u'tabs': [
         {
           u'type': u'tab',
           u'url': u'http://www.cnn.com',
           u'title': u'CNN',
           u'timestamp': 2129082.12098,
           u'direction': u'ltr'}]},
       {
         u'type': u'tab',
         u'url': u'http://www.altavista.com',
         u'title': u'Altavista',
         u'timestamp': 21390820.12903,
         u'direction': u'rtl'}]
    """
    return self._GetNTPInfo()['recently_closed']

  def GetNTPApps(self):
    """Retrieves information about the apps listed on the NTP.

    In the sample data below, the "launch_type" will be one of the following
    strings: "pinned", "regular", "fullscreen", "window", or "unknown".

    SAMPLE:
    [
      {
        u'app_launch_index': 2,
        u'description': u'Web Store',
        u'icon_big': u'chrome://theme/IDR_APP_DEFAULT_ICON',
        u'icon_small': u'chrome://favicon/https://chrome.google.com/webstore',
        u'id': u'ahfgeienlihckogmohjhadlkjgocpleb',
        u'is_component_extension': True,
        u'is_disabled': False,
        u'launch_container': 2,
        u'launch_type': u'regular',
        u'launch_url': u'https://chrome.google.com/webstore',
        u'name': u'Chrome Web Store',
        u'options_url': u'',
      },
      {
        u'app_launch_index': 1,
        u'description': u'A countdown app',
        u'icon_big': (u'chrome-extension://aeabikdlfbfeihglecobdkdflahfgcpd/'
                      u'countdown128.png'),
        u'icon_small': (u'chrome://favicon/chrome-extension://'
                        u'aeabikdlfbfeihglecobdkdflahfgcpd/'
                        u'launchLocalPath.html'),
        u'id': u'aeabikdlfbfeihglecobdkdflahfgcpd',
        u'is_component_extension': False,
        u'is_disabled': False,
        u'launch_container': 2,
        u'launch_type': u'regular',
        u'launch_url': (u'chrome-extension://aeabikdlfbfeihglecobdkdflahfgcpd/'
                        u'launchLocalPath.html'),
        u'name': u'Countdown',
        u'options_url': u'',
      }
    ]

    Returns:
      A list of dictionaries in which each dictionary contains the information
      for a single app that appears in the "Apps" section of the NTP.
    """
    return self._GetNTPInfo()['apps']

  def _GetNTPInfo(self):
    """Get info about the New Tab Page (NTP).

    This does not retrieve the actual info displayed in a particular NTP; it
    retrieves the current state of internal data that would be used to display
    an NTP.  This includes info about the apps, the most visited sites,
    the recently closed tabs and windows, and the default NTP sites.

    SAMPLE:
    {
      u'apps': [ ... ],
      u'most_visited': [ ... ],
      u'recently_closed': [ ... ],
      u'default_sites': [ ... ]
    }

    Returns:
      A dictionary containing all the NTP info. See details about the different
      sections in their respective methods: GetNTPApps(), GetNTPThumbnails(),
      GetNTPRecentlyClosed(), and GetNTPDefaultSites().

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
      'command': 'GetNTPInfo',
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def _CheckNTPThumbnailShown(self, thumbnail):
    if self.GetNTPThumbnailIndex(thumbnail) == -1:
      raise NTPThumbnailNotShownError()

  def InstallApp(self, app_crx_file_path):
    """Installs the specified app synchronously.

    An app file is a file with a .crx suffix, like an extension or theme.  The
    app file must be specified with an absolute path.  This method will not
    return until the app is installed.

    Returns:
      The ID of the installed app, on success.  The empty string, otherwise.
    """
    return self.InstallExtension(app_crx_file_path, False)

  def UninstallApp(self, app_id):
    """Uninstalls the specified app synchronously.

    Args:
      app_id: The string ID of the app to uninstall.  It can be retrieved
              through the call to GetNTPApps above.

    Returns:
      True, if the app was successfully uninstalled, or
      False, otherwise.
    """
    return self.UninstallExtensionById(app_id)

  def LaunchApp(self, app_id):
    """Opens the New Tab Page and launches the specified app from it.

    This method will not return until after the contents of a new tab for the
    launched app have stopped loading.

    Args:
      app_id: The string ID of the app to launch.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    self.AppendTab(GURL('chrome://newtab'))  # Also activates this tab.
    cmd_dict = {
      'command': 'LaunchApp',
      'id': app_id,
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def SetAppLaunchType(self, app_id, launch_type, windex=0):
    """Sets the launch type for the specified app.

    Args:
      app_id: The string ID of the app whose launch type should be set.
      launch_type: The string launch type, which must be one of the following:
                   'pinned': Launch in a pinned tab.
                   'regular': Launch in a regular tab.
                   'fullscreen': Launch in a fullscreen tab.
                   'window': Launch in a new browser window.
      windex: The index of the browser window to work on.  Defaults to 0 (the
              first window).

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    self.assertTrue(launch_type in ('pinned', 'regular', 'fullscreen',
                                    'window'),
                    msg='Unexpected launch type value: "%s"' % launch_type)
    cmd_dict = {
      'command': 'SetAppLaunchType',
      'id': app_id,
      'launch_type': launch_type,
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=windex)

  def KillRendererProcess(self, pid):
    """Kills the given renderer process.

    This will return only after the browser has received notice of the renderer
    close.

    Args:
      pid: the process id of the renderer to kill

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'KillRendererProcess',
        'pid': pid
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def GetNTPThumbnailMode(self):
    """Identifies whether or not each relevant NTP section is in thumbnail mode.

    Thumbnail mode applies to the Apps section and the Most Visited section.
    When in thumbnail mode, large thumbnails appear for each item in the
    section.  When not in thumbnail mode, small icons appear instead.  At any
    given time, at most one section can be in thumbnail mode in the NTP.

    SAMPLE OUTPUT:
    {
      u'apps': True,
      u'most_visited': False
    }

    Returns:
      A dictionary indicating whether or not each relevant section of the NTP
      is in thumbnail mode.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'GetNTPThumbnailMode',
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def SetNTPThumbnailMode(self, section, turn_on):
    """Puts or removes a section of the NTP into/from thumbnail (expanded) mode.

    Thumbnail mode applies to the Apps section and the Most Visited section.
    At any given time, at most one section can be in thumbnail mode in the NTP;
    when a specified section is put into thumbnail mode, the other section is
    removed from thumbnail mode.

    Args:
      section: A string representing the NTP section to use.
               Possible values:
                 'apps': the "Apps" section.
                 'most_visited': the "Most Visited" section.
      turn_on: A boolean indicating whether to put the section into thumbnail
               mode (True), or remove the section from thumbnail mode (False).

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'SetNTPThumbnailMode',
        'section': section,
        'turn_on': turn_on
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def GetNTPMenuMode(self):
    """Identifies whether or not each relevant NTP section is in menu mode.

    Menu mode applies to the Apps section, the Most Visited section, and the
    Recently Closed section.  When in menu mode, the section is almost
    completely hidden, appearing as a menu at the bottom of the NTP.  When not
    in menu mode, the section appears with all information in the regular
    location in the NTP.

    SAMPLE OUTPUT:
    {
      u'apps': False,
      u'most_visited': True,
      u'recently_closed': True
    }

    Returns:
      A dictionary indicating whether or not each relevant section of the NTP
      is in menu mode.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'GetNTPMenuMode',
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  def SetNTPMenuMode(self, section, turn_on):
    """Puts or removes the specified section of the NTP into/from menu mode.

    Menu mode applies to the Apps section, the Most Visited section, and the
    Recently Closed section.

    Args:
      section: A string representing the NTP section to use.
               Possible values:
                 'apps': the "Apps" section.
                 'most_visited': the "Most Visited" section.
                 'recently_closed': the "Recently Closed" section.
      turn_on: A boolean indicating whether to put the section into menu mode
               (True), or remove the section from menu mode (False).

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'SetNTPMenuMode',
        'section': section,
        'turn_on': turn_on
    }
    return self._GetResultFromJSONRequest(cmd_dict)

  ## ChromeOS section

  def GetLoginInfo(self):
    """Returns information about login and screen locker state.

    This includes things like whether a user is logged in, the username
    of the logged in user, and whether the screen is locked.

    Returns:
      A dictionary.
      Sample:
      { u'is_guest': False,
        u'is_owner': True,
        u'email': u'example@gmail.com',
        u'is_screen_locked': False,
        u'is_logged_in': True}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetLoginInfo' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def _WaitForSessionManagerRestart(self, function):
    """Call a function and wait for the ChromeOS session_manager to restart.

    Args:
      function: The function to call.
    """
    assert callable(function)
    pgrep_process = subprocess.Popen(['pgrep', 'session_manager'],
                                     stdout=subprocess.PIPE)
    old_pid = pgrep_process.communicate()[0].strip()
    function()
    return self.WaitUntil(lambda: self._IsSessionManagerReady(old_pid))

  def _WaitForInodeChange(self, path, function):
    """Call a function and wait for the specified file path to change.

    Args:
      path: The file path to check for changes.
      function: The function to call.
    """
    assert callable(function)
    old_inode = os.stat(path).st_ino
    function()
    return self.WaitUntil(lambda: self._IsInodeNew(path, old_inode))

  def ShowCreateAccountUI(self):
    """Go to the account creation page.

    This is the same as clicking the "Create Account" link on the
    ChromeOS login screen. Does not actually create a new account.
    Should be displaying the login screen to work.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'ShowCreateAccountUI' }
    # See note below under LoginAsGuest(). ShowCreateAccountUI() logs
    # the user in as guest in order to access the account creation page.
    assert self._WaitForInodeChange(
        self._named_channel_id,
        lambda: self._GetResultFromJSONRequest(cmd_dict, windex=-1)), \
        'Chrome did not reopen the testing channel after login as guest.'
    self.SetUp()

  def LoginAsGuest(self):
    """Login to chromeos as a guest user.

    Waits until logged in.
    Should be displaying the login screen to work.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'LoginAsGuest' }
    # Currently, logging in as guest causes session_manager to
    # restart Chrome, which will close the testing channel.
    # We need to call SetUp() again to reconnect to the new channel.
    assert self._WaitForInodeChange(
        self._named_channel_id,
        lambda: self._GetResultFromJSONRequest(cmd_dict, windex=-1)), \
        'Chrome did not reopen the testing channel after login as guest.'
    self.SetUp()

  def Login(self, username, password):
    """Login to chromeos.

    Waits until logged in.
    Should be displaying the login screen to work.

    Returns:
      An error string if an error occured.
      None otherwise.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'Login',
        'username': username,
        'password': password,
    }
    result = self._GetResultFromJSONRequest(cmd_dict, windex=-1)
    return result.get('error_string')

  def Logout(self):
    """Log out from ChromeOS and wait for session_manager to come up.

    May return before logout is complete and
    gives no indication of success or failure.
    Should be logged in to work.
    """
    assert self.GetLoginInfo()['is_logged_in'], \
        'Trying to log out when already logged out.'
    assert self._WaitForSessionManagerRestart(
        lambda: self.ApplyAccelerator(IDC_EXIT)), \
        'Session manager did not restart after logout.'

    self.setUp()

  def LockScreen(self):
    """Locks the screen on chromeos.

    Waits until screen is locked.
    Should be logged in and screen should not be locked to work.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'LockScreen' }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def UnlockScreen(self, password):
    """Unlocks the screen on chromeos, authenticating the user's password first.

    Waits until screen is unlocked.
    Screen locker should be active for this to work.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'UnlockScreen',
        'password': password,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SignoutInScreenLocker(self):
    """Signs out of chromeos using the screen locker's "Sign out" feature.

    Effectively the same as clicking the "Sign out" link on the screen locker.
    Screen should be locked for this to work.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'SignoutInScreenLocker' }
    assert self._WaitForSessionManagerRestart(
        lambda: self._GetResultFromJSONRequest(cmd_dict, windex=-1)), \
        'Session manager did not restart after logout.'
    self.setUp()

  def GetBatteryInfo(self):
    """Get details about battery state.

    Returns:
      A dictionary with the following keys:

      'battery_is_present': bool
      'line_power_on': bool
      if 'battery_is_present':
        'battery_percentage': float (0 ~ 100)
        'battery_fully_charged': bool
        if 'line_power_on':
          'battery_time_to_full': int (seconds)
        else:
          'battery_time_to_empty': int (seconds)

      If it is still calculating the time left, 'battery_time_to_full'
      and 'battery_time_to_empty' will be absent.

      Use 'battery_fully_charged' instead of 'battery_percentage'
      or 'battery_time_to_full' to determine whether the battery
      is fully charged, since the percentage is only approximate.

      Sample:
        { u'battery_is_present': True,
          u'line_power_on': False,
          u'battery_time_to_empty': 29617,
          u'battery_percentage': 100.0,
          u'battery_fully_charged': False }

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetBatteryInfo' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def GetNetworkInfo(self):
    """Get details about ethernet, wifi, and cellular networks on chromeos.

    Returns:
      A dictionary.
      Sample:
      { u'connected_ethernet': u'/profile/default/ethernet_abcd',
        u'connected_wifi': u'/profile/default/wifi_abcd_1234_managed_none',
        u'ethernet_networks':
            { u'/profile/default/ethernet_abcd':
                { u'device_path': u'/device/abcdeth',
                  u'ip_address': u'11.22.33.44',
                  u'name': u'',
                  u'service_path':
                  u'/profile/default/ethernet_abcd',
                  u'status': u'Connected'}},
        u'ip_address': u'11.22.33.44',
        u'wifi_networks':
            { u'/profile/default/wifi_abcd_1234_managed_none':
                { u'device_path': u'/device/abcdwifi',
                  u'encrypted': False,
                  u'encryption': u'',
                  u'ip_address': u'123.123.123.123',
                  u'name': u'WifiNetworkName1',
                  u'status': u'Connected',
                  u'strength': 76},
              u'/profile/default/wifi_abcd_1234_managed_802_1x':
                  { u'device_path': u'/device/abcdwifi',
                    u'encrypted': True,
                    u'encryption': u'8021X',
                    u'ip_address': u'',
                    u'name': u'WifiNetworkName2',
                    u'service_path':
                    u'status': u'Idle',
                    u'strength': 79}}}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetNetworkInfo' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def NetworkScan(self):
    """Causes ChromeOS to scan for available wifi networks.

    Blocks until scanning is complete.

    Returns:
      The new list of networks obtained from GetNetworkInfo().

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'NetworkScan' }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)
    return self.GetNetworkInfo()

  PROXY_TYPE_DIRECT = 1
  PROXY_TYPE_MANUAL = 2
  PROXY_TYPE_PAC = 3

  def GetProxyTypeName(self, proxy_type):
    values = { self.PROXY_TYPE_DIRECT: 'Direct Internet connection',
               self.PROXY_TYPE_MANUAL: 'Manual proxy configuration',
               self.PROXY_TYPE_PAC: 'Automatic proxy configuration' }
    return values[proxy_type]

  def GetProxySettingsOnChromeOS(self):
    """Get current proxy settings on Chrome OS.

    Returns:
      A dictionary. See SetProxySettings() below
      for the full list of possible dictionary keys.

      Samples:
      { u'ignorelist': [],
        u'single': False,
        u'type': 1}

      { u'ignorelist': [u'www.example.com', u'www.example2.com'],
        u'single': True,
        u'singlehttp': u'24.27.78.152',
        u'singlehttpport': 1728,
        u'type': 2}

      { u'ignorelist': [],
        u'pacurl': u'http://example.com/config.pac',
        u'single': False,
        u'type': 3}

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetProxySettings' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SetProxySettingsOnChromeOS(self, key, value):
    """Set a proxy setting on Chrome OS.

    Owner must be logged in for these to persist.
    If user is not logged in or is logged in as non-owner or guest,
    proxy settings do not persist across browser restarts or login/logout.

    Valid settings are:
      'type': int - Type of proxy. Should be one of:
                     PROXY_TYPE_DIRECT, PROXY_TYPE_MANUAL, PROXY_TYPE_PAC.
      'ignorelist': list - The list of hosts and domains to ignore.

      These settings set 'type' to PROXY_TYPE_MANUAL:
        'single': boolean - Whether to use the same proxy for all protocols.

        These settings set 'single' to True:
          'singlehttp': string - If single is true, the proxy address to use.
          'singlehttpport': int - If single is true, the proxy port to use.

        These settings set 'single' to False:
          'httpurl': string - HTTP proxy address.
          'httpport': int - HTTP proxy port.
          'httpsurl': string - Secure HTTP proxy address.
          'httpsport': int - Secure HTTP proxy port.
          'ftpurl': string - FTP proxy address.
          'ftpport': int - FTP proxy port.
          'socks': string - SOCKS host address.
          'socksport': int - SOCKS host port.

      This setting sets 'type' to PROXY_TYPE_PAC:
        'pacurl': string - Autoconfiguration URL.

    Examples:
      # Sets direct internet connection, no proxy.
      self.SetProxySettings('type', self.PROXY_TYPE_DIRECT)

      # Sets manual proxy configuration, same proxy for all protocols.
      self.SetProxySettings('singlehttp', '24.27.78.152')
      self.SetProxySettings('singlehttpport', 1728)
      self.SetProxySettings('ignorelist', ['www.example.com', 'example2.com'])

      # Sets automatic proxy configuration with the specified PAC url.
      self.SetProxySettings('pacurl', 'http://example.com/config.pac')

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'SetProxySettings',
        'key': key,
        'value': value,
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def ConnectToWifiNetwork(self, service_path,
                           password='', identity='', certpath=''):
    """Connect to a wifi network by its service path.

    Blocks until connection succeeds or fails.

    Args:
      service_path: Flimflam path that defines the wifi network.
      password: Passphrase for connecting to the wifi network.
      identity: Identity for 802.11x networks.
      certpath: Certificate path for 802.11x networks.

    Returns:
      An error string if an error occured.
      None otherwise.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'ConnectToWifiNetwork',
        'service_path': service_path,
        'password': password,
        'identity': identity,
        'certpath': certpath,
    }
    result = self._GetResultFromJSONRequest(cmd_dict, windex=-1, timeout=50000)
    return result.get('error_string')

  def ConnectToHiddenWifiNetwork(self, ssid, security, password=''):
    """Connect to a wifi network by its service path.

    Blocks until connection succeeds or fails.

    Args:
      ssid: The SSID of the network to connect to.
      security: The network's security type. One of: 'SECURITY_NONE',
                'SECURITY_WEP', 'SECURITY_WPA', 'SECURITY_RSN', 'SECURITY_8021X'
      password: Passphrase for connecting to the wifi network.

    Returns:
      An error string if an error occured.
      None otherwise.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    assert security in ('SECURITY_NONE', 'SECURITY_WEP', 'SECURITY_WPA',
                        'SECURITY_RSN', 'SECURITY_8021X')
    cmd_dict = {
        'command': 'ConnectToHiddenWifiNetwork',
        'ssid': ssid,
        'security': security,
        'password': password,
    }
    result = self._GetResultFromJSONRequest(cmd_dict, windex=-1, timeout=50000)
    return result.get('error_string')

  def DisconnectFromWifiNetwork(self):
    """Disconnect from the connected wifi network.

    Blocks until disconnect is complete.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = {
        'command': 'DisconnectFromWifiNetwork',
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def GetUpdateInfo(self):
    """Gets the status of the ChromeOS updater.

    Returns:
      a dictionary.
      Samples:
      { u'status': u'idle',
        u'release_track': u'beta-channel'}

      { u'status': u'downloading',
        u'release_track': u'beta-channel',
        u'download_progress': 0.1203236708350371,   # 0.0 ~ 1.0
        u'new_size': 152033593,                     # size of payload, in bytes
        u'last_checked_time': 1302055709}           # seconds since UNIX epoch

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetUpdateInfo' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def UpdateCheck(self):
    """Checks for a ChromeOS update. Blocks until finished updating.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'UpdateCheck' }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SetReleaseTrack(self, track):
    """Sets the release track (channel) of the ChromeOS updater.

    Valid values for the track parameter are:
      'stable-channel', 'beta-channel', 'dev-channel'

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    assert track in ('stable-channel', 'beta-channel', 'dev-channel'), \
        'Attempt to set release track to unknown release track "%s".' % track
    cmd_dict = {
        'command': 'SetReleaseTrack',
        'track': track,
    }
    self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def GetVolumeInfo(self):
    """Gets the volume and whether the device is muted.

    Returns:
      a tuple.
      Sample:
      (47.763456790123456, False)

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'GetVolumeInfo' }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SetVolume(self, volume):
    """Sets the volume on ChromeOS. Only valid if not muted.

    Args:
      volume: The desired volume level as a percent from 0 to 100.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    assert volume >= 0 and volume <= 100
    cmd_dict = {
        'command': 'SetVolume',
        'volume': float(volume),
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  def SetMute(self, mute):
    """Sets whether ChromeOS is muted or not.

    Args:
      mute: True to mute, False to unmute.

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    cmd_dict = { 'command': 'SetMute' }
    cmd_dict = {
        'command': 'SetMute',
        'mute': mute,
    }
    return self._GetResultFromJSONRequest(cmd_dict, windex=-1)

  ## ChromeOS section -- end


class PyUITestSuite(pyautolib.PyUITestSuiteBase, unittest.TestSuite):
  """Base TestSuite for PyAuto UI tests."""

  def __init__(self, args):
    pyautolib.PyUITestSuiteBase.__init__(self, args)

    # Figure out path to chromium binaries
    browser_dir = os.path.normpath(os.path.dirname(pyautolib.__file__))
    logging.debug('Loading pyauto libs from %s', browser_dir)
    self.Initialize(pyautolib.FilePath(browser_dir))
    os.environ['PATH'] = browser_dir + os.pathsep + os.environ['PATH']

    unittest.TestSuite.__init__(self)
    cr_source_root = os.path.normpath(os.path.join(
        os.path.dirname(__file__), os.pardir, os.pardir, os.pardir))
    self.SetCrSourceRoot(pyautolib.FilePath(cr_source_root))

    # Start http server, if needed.
    global _OPTIONS
    if not _OPTIONS.no_http_server:
      self._StartHTTPServer()

  def __del__(self):
    # python unittest module is setup such that the suite gets deleted before
    # the test cases, which is odd because our test cases depend on
    # initializtions like exitmanager, autorelease pool provided by the
    # suite. Forcibly delete the test cases before the suite.
    del self._tests
    pyautolib.PyUITestSuiteBase.__del__(self)

    global _HTTP_SERVER
    if _HTTP_SERVER:
      self._StopHTTPServer()

  def _StartHTTPServer(self):
    """Start a local file server hosting data files over http://"""
    global _HTTP_SERVER
    assert not _HTTP_SERVER, 'HTTP Server already started'
    http_data_dir = _OPTIONS.http_data_dir
    http_server = pyautolib.TestServer(pyautolib.TestServer.TYPE_HTTP,
        pyautolib.FilePath(http_data_dir))
    assert http_server.Start(), 'Could not start http server'
    _HTTP_SERVER = http_server
    logging.debug('Started http server at "%s".' % http_data_dir)

  def _StopHTTPServer(self):
    """Stop the local http server."""
    global _HTTP_SERVER
    assert _HTTP_SERVER, 'HTTP Server not yet started'
    assert _HTTP_SERVER.Stop(), 'Could not stop http server'
    _HTTP_SERVER = None
    logging.debug('Stopped http server.')


class _GTestTextTestResult(unittest._TextTestResult):
  """A test result class that can print formatted text results to a stream.

  Results printed in conformance with gtest output format, like:
  [ RUN        ] autofill.AutofillTest.testAutofillInvalid: "test desc."
  [         OK ] autofill.AutofillTest.testAutofillInvalid
  [ RUN        ] autofill.AutofillTest.testFillProfile: "test desc."
  [         OK ] autofill.AutofillTest.testFillProfile
  [ RUN        ] autofill.AutofillTest.testFillProfileCrazyCharacters: "Test."
  [         OK ] autofill.AutofillTest.testFillProfileCrazyCharacters
  """
  def __init__(self, stream, descriptions, verbosity):
    unittest._TextTestResult.__init__(self, stream, descriptions, verbosity)

  def _GetTestURI(self, test):
    if sys.version_info[:2] <= (2, 4):
      return '%s.%s' % (unittest._strclass(test.__class__),
                        test._TestCase__testMethodName)
    return '%s.%s' % (unittest._strclass(test.__class__), test._testMethodName)

  def getDescription(self, test):
    return '%s: "%s"' % (self._GetTestURI(test), test.shortDescription())

  def startTest(self, test):
    unittest.TestResult.startTest(self, test)
    self.stream.writeln('[ RUN        ] %s' % self.getDescription(test))

  def addSuccess(self, test):
    unittest.TestResult.addSuccess(self, test)
    self.stream.writeln('[         OK ] %s' % self._GetTestURI(test))

  def addError(self, test, err):
    unittest.TestResult.addError(self, test, err)
    self.stream.writeln('[      ERROR ] %s' % self._GetTestURI(test))

  def addFailure(self, test, err):
    unittest.TestResult.addFailure(self, test, err)
    self.stream.writeln('[     FAILED ] %s' % self._GetTestURI(test))


class PyAutoTextTestRuner(unittest.TextTestRunner):
  """Test Runner for PyAuto tests that displays results in textual format.

  Results are displayed in conformance with gtest output.
  """
  def __init__(self, verbosity=1):
    unittest.TextTestRunner.__init__(self,
                                     stream=sys.stderr,
                                     verbosity=verbosity)

  def _makeResult(self):
    return _GTestTextTestResult(self.stream, self.descriptions, self.verbosity)


# Implementation inspired from unittest.main()
class Main(object):
  """Main program for running PyAuto tests."""

  _options, _args = None, None
  _tests_filename = 'PYAUTO_TESTS'
  _platform_map = {
    'win32':  'win',
    'darwin': 'mac',
    'linux2': 'linux',
    'chromeos': 'chromeos',
  }

  def __init__(self):
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parse command line args."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-v', '--verbose', action='store_true', default=False,
        help='Make PyAuto verbose.')
    parser.add_option(
        '', '--log-file', type='string', default=None,
        help='Provide a path to a file to which the logger will log')
    parser.add_option(
        '-D', '--wait-for-debugger', action='store_true', default=False,
        help='Block PyAuto on startup for attaching debugger.')
    parser.add_option(
        '', '--chrome-flags', type='string', default='',
        help='Flags passed to Chrome.  This is in addition to the usual flags '
             'like suppressing first-run dialogs, enabling automation.  '
             'See chrome/common/chrome_switches.cc for the list of flags '
             'chrome understands.')
    parser.add_option(
        '', '--list-missing-tests', action='store_true', default=False,
        help='Print a list of tests not included in PYAUTO_TESTS, and exit')
    parser.add_option(
        '', '--repeat', type='int', default=1,
        help='Number of times to repeat the tests. Useful to determine '
             'flakiness. Defaults to 1.')
    parser.add_option(
        '-S', '--suite', type='string', default='FULL',
        help='Name of the suite to load.  Defaults to "FULL".')
    parser.add_option(
        '-L', '--list-tests', action='store_true', default=False,
        help='List all tests, and exit.')
    parser.add_option(
        '', '--no-http-server', action='store_true', default=False,
        help='Do not start an http server to serve files in data dir.')
    parser.add_option(
        '', '--http-data-dir', type='string',
        default=os.path.join('chrome', 'test', 'data'),
        help='Relative path from which http server should serve files.')
    parser.add_option(
        '', '--channel-id', type='string', default='',
        help='Name of channel id, if using named interface.')

    self._options, self._args = parser.parse_args()
    global _OPTIONS
    _OPTIONS = self._options  # export options so other classes can access

    # Setup logging - start with defaults
    level = logging.INFO
    format = None

    if self._options.verbose:
      level=logging.DEBUG
      format='%(asctime)s %(levelname)-8s %(message)s'

    logging.basicConfig(level=level, format=format,
                        filename=self._options.log_file)

    if self._options.list_missing_tests:
      self._ListMissingTests()
      sys.exit(0)

  def TestsDir(self):
    """Returns the path to dir containing tests.

    This is typically the dir containing the tests description file.
    This method should be overridden by derived class to point to other dirs
    if needed.
    """
    return os.path.dirname(__file__)

  @staticmethod
  def _ImportTestsFromName(name):
    """Get a list of all test names from the given string.

    Args:
      name: dot-separated string for a module, a test case or a test method.
            Examples: omnibox  (a module)
                      omnibox.OmniboxTest  (a test case)
                      omnibox.OmniboxTest.testA  (a test method)

    Returns:
      [omnibox.OmniboxTest.testA, omnibox.OmniboxTest.testB, ...]
    """
    def _GetTestsFromTestCase(class_obj):
      """Return all test method names from given class object."""
      return [class_obj.__name__ + '.' + x for x in dir(class_obj) if
              x.startswith('test')]

    def _GetTestsFromModule(module):
      """Return all test method names from the given module object."""
      tests = []
      for name in dir(module):
        obj = getattr(module, name)
        if (isinstance(obj, (type, types.ClassType)) and
            issubclass(obj, PyUITest) and obj != PyUITest):
          tests.extend([module.__name__ + '.' + x for x in
                        _GetTestsFromTestCase(obj)])
      return tests

    module = None
    # Locate the module
    parts = name.split('.')
    parts_copy = parts[:]
    while parts_copy:
      try:
        module = __import__('.'.join(parts_copy))
        break
      except ImportError:
        del parts_copy[-1]
        if not parts_copy: raise
    # We have the module. Pick the exact test method or class asked for.
    parts = parts[1:]
    obj = module
    for part in parts:
      obj = getattr(obj, part)

    if type(obj) == types.ModuleType:
      return _GetTestsFromModule(obj)
    elif (isinstance(obj, (type, types.ClassType)) and
          issubclass(obj, PyUITest) and obj != PyUITest):
      return [module.__name__ + '.' + x for x in _GetTestsFromTestCase(obj)]
    elif type(obj) == types.UnboundMethodType:
      return [name]
    else:
      logging.warn('No tests in "%s"' % name)
      return []

  def _ListMissingTests(self):
    """Print tests missing from PYAUTO_TESTS."""
    # Fetch tests from all test scripts
    all_test_files = filter(lambda x: x.endswith('.py'),
                            os.listdir(self.TestsDir()))
    all_tests_modules = [os.path.splitext(x)[0] for x in all_test_files]
    all_tests = reduce(lambda x, y: x + y,
                       map(self._ImportTestsFromName, all_tests_modules))
    # Fetch tests included by PYAUTO_TESTS
    pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
    pyauto_tests = reduce(lambda x, y: x + y,
                          map(self._ImportTestsFromName,
                              self._ExpandTestNamesFrom(pyauto_tests_file,
                                                        self._options.suite)))
    for a_test in all_tests:
      if a_test not in pyauto_tests:
        print a_test

  def _HasTestCases(self, module_string):
    """Determines if we have any PyUITest test case classes in the module
       identified by |module_string|."""
    module = __import__(module_string)
    for name in dir(module):
      obj = getattr(module, name)
      if (isinstance(obj, (type, types.ClassType)) and
          issubclass(obj, PyUITest)):
        return True
    return False

  def _ExpandTestNames(self, args):
    """Returns a list of tests loaded from the given args.

    The given args can be either a module (ex: module1) or a testcase
    (ex: module2.MyTestCase) or a test (ex: module1.MyTestCase.testX)
    If empty, the tests in the already imported modules are loaded.

    Args:
      args: [module1, module2, module3.testcase, module4.testcase.testX]
            These modules or test cases or tests should be importable

      Returns:
        a list of expanded test names.  Example:
          [
            'module1.TestCase1.testA',
            'module1.TestCase1.testB',
            'module2.TestCase2.testX',
            'module3.testcase.testY',
            'module4.testcase.testX'
          ]
    """
    if not args:  # Load tests ourselves
      if self._HasTestCases('__main__'):    # we are running a test script
        module_name = os.path.splitext(os.path.basename(sys.argv[0]))[0]
        args.append(module_name)   # run the test cases found in it
      else:  # run tests from the test description file
        pyauto_tests_file = os.path.join(self.TestsDir(), self._tests_filename)
        logging.debug("Reading %s", pyauto_tests_file)
        if not os.path.exists(pyauto_tests_file):
          logging.warn("%s missing. Cannot load tests." % pyauto_tests_file)
        else:
          args = self._ExpandTestNamesFrom(pyauto_tests_file,
                                           self._options.suite)
    return args

  def _ExpandTestNamesFrom(self, filename, suite):
    """Load test names from the given file.

    Args:
      filename: the file to read the tests from
      suite: the name of the suite to load from |filename|.

    Returns:
      a list of test names
      [module.testcase.testX, module.testcase.testY, ..]
    """
    suites = PyUITest.EvalDataFrom(filename)
    platform = sys.platform
    if PyUITest.IsChromeOS():  # check if it's chromeos
      platform = 'chromeos'
    assert platform in self._platform_map, '%s unsupported' % platform
    def _NamesInSuite(suite_name):
      logging.debug('Expanding suite %s' % suite_name)
      platforms = suites.get(suite_name)
      names = platforms.get('all', []) + \
              platforms.get(self._platform_map[platform], [])
      ret = []
      # Recursively include suites if any.  Suites begin with @.
      for name in names:
        if name.startswith('@'):  # Include another suite
          ret.extend(_NamesInSuite(name[1:]))
        else:
          ret.append(name)
      return ret

    assert suite in suites, '%s: No such suite in %s' % (suite, filename)
    all_names = _NamesInSuite(suite)
    args = []
    excluded = []
    # Find all excluded tests.  Excluded tests begin with '-'.
    for name in all_names:
      if name.startswith('-'):  # Exclude
        excluded.extend(self._ImportTestsFromName(name[1:]))
      else:
        args.extend(self._ImportTestsFromName(name))
    for name in excluded:
      if name in args:
        args.remove(name)
      else:
        logging.warn('Cannot exclude %s. Not included. Ignoring' % name)
    if excluded:
      logging.debug('Excluded %d test(s): %s' % (len(excluded), excluded))
    return args

  def _Run(self):
    """Run the tests."""
    if self._options.wait_for_debugger:
      raw_input('Attach debugger to process %s and hit <enter> ' % os.getpid())

    suite_args = [sys.argv[0]]
    chrome_flags = self._options.chrome_flags
    # Set CHROME_HEADLESS. It enables crash reporter on posix.
    os.putenv('CHROME_HEADLESS', '1')
    os.putenv('EXTRA_CHROME_FLAGS', chrome_flags)
    pyauto_suite = PyUITestSuite(suite_args)
    test_names = self._ExpandTestNames(self._args)
    test_names *= self._options.repeat
    logging.debug("Loading %d tests from %s", len(test_names), test_names)
    if self._options.list_tests:  # List tests and exit
      for name in test_names:
        print name
      sys.exit(0)
    loaded_tests = unittest.defaultTestLoader.loadTestsFromNames(test_names)
    pyauto_suite.addTests(loaded_tests)
    verbosity = 1
    if self._options.verbose:
      verbosity = 2
    result = PyAutoTextTestRuner(verbosity=verbosity).run(pyauto_suite)
    del loaded_tests  # Need to destroy test cases before the suite
    del pyauto_suite
    sys.exit(not result.wasSuccessful())


if __name__ == '__main__':
  Main()
