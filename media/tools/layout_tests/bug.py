# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Bug module that is necessary for the layout analyzer."""

import re


class Bug(object):
  """A class representing a bug.

  TODO(imasaki): add more functionalities here if bug-tracker API is available.
  For example, you can get the name of a bug owner.
  """
  CHROME_BUG_URL = 'http://crbug.com/'
  WEBKIT_BUG_URL = 'http://webkit.org/b/'
  # Type enum for the bug.
  WEBKIT = 0
  CHROMIUM = 1
  OTHERS = 2

  def __init__(self, bug_modifier):
    """Initialize the object using raw bug text (such as BUGWK2322).

    The bug modifier used in the test expectation file.

    Args:
      bug_modifier: a string representing a bug modifier. According to
        http://trac.webkit.org/wiki/TestExpectations#Modifiers,
        currently, BUGWK12345, BUGCR12345, BUGV8_12345, BUGDPRANKE are
        possible.
    """
    self.bug_txt = bug_modifier
    pattern_for_webkit_bug = r'BUGWK(\d+)'
    match = re.search(pattern_for_webkit_bug, bug_modifier)
    if match:
      self.type = self.WEBKIT
      self.url = self.WEBKIT_BUG_URL + match.group(1)
      return
    pattern_for_chrome_bug = r'BUGCR(\d+)'
    match = re.search(pattern_for_chrome_bug, bug_modifier)
    if match:
      self.type = self.CHROMIUM
      self.url = self.CHROME_BUG_URL + match.group(1)
      return
    pattern_for_other_bug = r'BUG(\S+)'
    match = re.search(pattern_for_other_bug, bug_modifier)
    if match:
      self.type = self.OTHERS
      self.url = 'mailto:%s@chromium.org' % match.group(1).lower()
      return
    self.url = ''

  def __str__(self):
    """Get a string representation of a bug object.

    Returns:
      a string for HTML link representation of a bug.
    """
    return '<a href="%s">%s</a>' % (self.url, self.bug_txt)
