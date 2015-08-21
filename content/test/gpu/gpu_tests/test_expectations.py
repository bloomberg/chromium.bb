# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch

# Valid expectation conditions are:
#
# Operating systems:
#     win, xp, vista, win7, mac, leopard, snowleopard, lion, mountainlion,
#     mavericks, yosemite, linux, chromeos, android
#
# Browser types:
#     android-webview-shell, android-content-shell, debug, release
#
# Sample usage in SetExpectations in subclasses:
#   self.Fail('gl-enable-vertex-attrib.html',
#       ['mac', 'release'], bug=123)

OS_CONDITIONS = ['win', 'xp', 'vista', 'win7',
                 'mac', 'leopard', 'snowleopard', 'lion', 'mountainlion',
                 'mavericks', 'yosemite', 'linux', 'chromeos', 'android']

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
    self.name_pattern = pattern
    self.url_pattern = pattern
    self.bug = bug

    self.os_conditions = []
    self.browser_conditions = []

    # Make sure that non-absolute paths are searchable
    if not '://' in self.url_pattern:
      self.url_pattern = '*/' + self.url_pattern

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
    self.expectations = []
    self.SetExpectations()

  def SetExpectations(self):
    """Called on creation. Override to set up custom expectations."""
    pass

  def Fail(self, url_pattern, conditions=None, bug=None):
    self._Expect('fail', url_pattern, conditions, bug)

  def Skip(self, url_pattern, conditions=None, bug=None):
    self._Expect('skip', url_pattern, conditions, bug)

  def _Expect(self, expectation, url_pattern, conditions=None, bug=None):
    self.expectations.append(self.CreateExpectation(expectation, url_pattern,
                                                    conditions, bug))

  def CreateExpectation(self, expectation, url_pattern, conditions=None,
                        bug=None):
    return Expectation(expectation, url_pattern, conditions, bug)

  def _GetExpectationObjectForPage(self, browser, page):
    for e in self.expectations:
      if self.ExpectationAppliesToPage(e, browser, page):
        return e
    return None

  def GetExpectationForPage(self, browser, page):
    e = self._GetExpectationObjectForPage(browser, page)
    if e:
      return e.expectation
    return 'pass'

  def ExpectationAppliesToPage(self, expectation, browser, page):
    """Defines whether the given expectation applies to the given page.

    Override this in subclasses to add more conditions. Call the
    superclass's implementation first, and return false if it returns
    false.

    Args:
      expectation: an instance of a subclass of Expectation, created
          by a call to CreateExpectation.
      browser: the currently running browser.
      page: the page to be run.
    """
    matches_url = fnmatch.fnmatch(page.url, expectation.url_pattern)
    matches_name = page.name and fnmatch.fnmatch(page.name,
                                                 expectation.name_pattern)
    if not (matches_url or matches_name):
      return False

    platform = browser.platform
    os_matches = (not expectation.os_conditions or
        platform.GetOSName() in expectation.os_conditions or
        platform.GetOSVersionName() in expectation.os_conditions)

    browser_matches = (
      (not expectation.browser_conditions) or
      browser.browser_type in expectation.browser_conditions)

    return os_matches and browser_matches
