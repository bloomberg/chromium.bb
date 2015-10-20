# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import urlparse

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, win8, win10, mac, leopard, snowleopard,
#     lion, mountainlion, mavericks, yosemite, linux, chromeos,
#     android
#
# Browser types:
#     android-webview-shell, android-content-shell, debug, release
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'release'], bug=123)

WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = ['leopard', 'snowleopard', 'lion', 'mountainlion',
                 'mavericks', 'yosemite']

OS_CONDITIONS = ['win', 'mac', 'linux', 'chromeos', 'android'] + \
                WIN_CONDITIONS + MAC_CONDITIONS

BROWSER_TYPE_CONDITIONS = [
    'android-webview-shell', 'android-content-shell', 'debug', 'release' ]

class Expectation(object):
  """Represents a single test expectation for a page.

  Supports conditions based on operating system (e.g., win, mac) and
  browser type (e.g. 'debug', 'release').

  Subclass this class and call super.__init__ last in your constructor
  in order to add new user-defined conditions. The conditions are
  parsed at the end of this class's constructor, so be careful not to
  overwrite the results of the constructor call!
  """

  def __init__(self, expectation, pattern, conditions=None, bug=None):
    self.expectation = expectation.lower()
    self.pattern = pattern
    self.bug = bug

    self.os_conditions = []
    self.browser_conditions = []

    if conditions:
      for c in conditions:
        self.ParseCondition(c)

  def ParseCondition(self, condition):
    """Parses a single test expectation condition.

    Can be overridden to handle new types of conditions. Call the
    superclass's implementation of ParseCondition at the end of your
    subclass if you don't handle the condition. The base
    implementation will raise an exception if the condition is
    unsupported.

    Valid expectation conditions are:

    Operating systems:
      win, xp, vista, win7, mac, leopard, snowleopard, lion,
      mountainlion, mavericks, yosemite, linux, chromeos, android

    Browser types:
      android-webview-shell, android-content-shell, debug, release

    Sample usage in SetExpectations in subclasses:
      self.Fail('gl-enable-vertex-attrib.html',
         ['mac', 'release'], bug=123)
    """
    cl = condition.lower()
    if cl in OS_CONDITIONS:
      self.os_conditions.append(cl)
    elif cl in BROWSER_TYPE_CONDITIONS:
      self.browser_conditions.append(condition)
    else:
      raise ValueError('Unknown expectation condition: "%s"' % cl)


class TestExpectations(object):
  """A class which defines the expectations for a page set test execution"""

  def __init__(self):
    self._expectations = []
    self._skip_matching_names = False
    self._built_expectation_cache = True
    self._ClearExpectationsCache()
    self.SetExpectations()

  def SetExpectations(self):
    """Called on creation. Override to set up custom expectations."""
    pass

  def Fail(self, pattern, conditions=None, bug=None):
    self._Expect('fail', pattern, conditions, bug)

  def Skip(self, pattern, conditions=None, bug=None):
    self._Expect('skip', pattern, conditions, bug)

  def _Expect(self, expectation, pattern, conditions=None, bug=None):
    self._AddExpectation(self.CreateExpectation(expectation, pattern,
                                                conditions, bug))

  def _AddExpectation(self, expectation):
    '''Call this to add an expectation to the set.

    For use only by this class and subclasses. Do not call this directly.'''
    self._expectations.append(expectation)
    self._ClearExpectationsCache()


  def CreateExpectation(self, expectation, pattern, conditions=None,
                        bug=None):
    return Expectation(expectation, pattern, conditions, bug)

  def _ClearExpectationsCache(self):
    if self._built_expectation_cache:
      # Only those expectations which contain no wildcard characters
      # (those which the fnmatch module would expand).
      self._expectations_by_pattern = {}
      # The remaining expectations which require expansion.
      self._expectations_with_wildcards = []
      self._built_expectation_cache = False

  def ClearExpectationsCacheForTesting(self):
    '''For use only by unit tests.'''
    self._ClearExpectationsCache()

  def _HasWildcardCharacters(self, input_string):
    # Could make this more precise.
    return '*' in input_string or '+' in input_string

  def _BuildExpectationsCache(self, browser, page):
    # Turn off name matching while building the cache.
    self._skip_matching_names = True
    for e in self._expectations:
      if self.ExpectationAppliesToPage(e, browser, page):
        if (self._HasWildcardCharacters(e.pattern)):
          self._expectations_with_wildcards.append(e)
        else:
          if e.pattern in self._expectations_by_pattern:
            print "WARNING: Non-wildcard pattern collision for", pattern
          self._expectations_by_pattern[e.pattern] = e
    self._built_expectation_cache = True
    self._skip_matching_names = False

  def _GetNormalizedURL(self, url, browser):
    # Telemetry uses backslashes in its file:// URLs on Windows,
    # breaking matching of test expectations.
    if not browser.platform.GetOSName() == 'win':
      return url
    return url.replace('\\', '/')

  def _GetURLPath(self, url):
    components = urlparse.urlsplit(url)
    # For compatibility, the file:// scheme must be treated specially.
    # The top-level directory shows up in the netloc portion of the URL.
    if components[0] == 'file':
      url_path = components[1] + components[2]
    else:
      url_path = components[2]
    # Chop any leading slash since the expectations used by this class
    # assume that.
    if (url_path and url_path[0] == '/'):
      url_path = url_path[1:]
    # Python's urlsplit doesn't seem to handle query arguments for
    # file:// URLs properly. Split them off manually.
    query_index = url_path.find('?')
    if query_index > 0:
      url_path = url_path[0:query_index]
    return url_path

  def _GetExpectationObjectForPage(self, browser, page):
    if not self._built_expectation_cache:
      self._BuildExpectationsCache(browser, page)
    # First attempt to look up by the page's URL or name.
    e = None
    # Relative URL (common case).
    url = self._GetNormalizedURL(page.url, browser)
    url_path = self._GetURLPath(url)
    if url_path:
      e = self._expectations_by_pattern.get(url_path)
    if e:
      return e
    e = self._expectations_by_pattern.get(url)
    if e:
      return e
    if page.name:
      e = self._expectations_by_pattern.get(page.name)
    if e:
      return e
    # Fall back to scanning through the expectations containing
    # wildcards.
    for e in self._expectations_with_wildcards:
      if self.ExpectationAppliesToPage(e, browser, page):
        return e
    return None

  def GetAllNonWildcardExpectations(self):
    return [e for e in self._expectations
              if not self._HasWildcardCharacters(e.pattern)]


  def GetExpectationForPage(self, browser, page):
    '''Fetches the expectation that applies to the given page.

    The implementation of this function performs significant caching
    based on the browser's parameters, which are expected to remain
    unchanged from call to call. If this is not true, the method
    ClearExpectationsCacheForTesting is available to clear the cache;
    but file a bug if this is needed for any reason but testing.
    '''
    e = self._GetExpectationObjectForPage(browser, page)
    if e:
      return e.expectation
    return 'pass'

  def ExpectationAppliesToPage(self, expectation, browser, page):
    """Defines whether the given expectation applies to the given page.

    Override this in subclasses to add more conditions. Call the
    superclass's implementation first, and return false if it returns
    false. Subclasses must not consult the page's name or URL; that is
    the responsibility of the base class.

    Args:
      expectation: an instance of a subclass of Expectation, created
          by a call to CreateExpectation.
      browser: the currently running browser.
      page: the page to be run.
    """
    # While building the expectations cache we need to match
    # everything except the page's name or URL.
    if not self._skip_matching_names:
      # Relative URL.
      if not fnmatch.fnmatch(self._GetURLPath(page.url),
                             expectation.pattern):
        # Absolute URL.
        if not fnmatch.fnmatch(page.url,
                               expectation.pattern):
          # Name.
          if not (page.name and fnmatch.fnmatch(page.name,
                                                expectation.pattern)):
            return False

    platform = browser.platform
    os_matches = (not expectation.os_conditions or
        platform.GetOSName() in expectation.os_conditions or
        platform.GetOSVersionName() in expectation.os_conditions)

    browser_matches = (
      (not expectation.browser_conditions) or
      browser.browser_type in expectation.browser_conditions)

    return os_matches and browser_matches
