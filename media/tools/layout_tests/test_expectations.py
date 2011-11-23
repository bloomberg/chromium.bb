# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to analyze test expectations for Webkit layout tests."""

import re
import urllib2

# Default Webkit SVN location for chromium test expectation file.
# TODO(imasaki): support multiple test expectations files.
DEFAULT_TEST_EXPECTATION_LOCATION = (
    'http://svn.webkit.org/repository/webkit/trunk/'
    'LayoutTests/platform/chromium/test_expectations.txt')

# The following is from test expectation syntax. The detail can be found in
# http://www.chromium.org/developers/testing/
# webkit-layout-tests#TOC-Test-Expectations
# <decision> ::== [SKIP] [WONTFIX] [SLOW]
DECISION_NAMES = ['SKIP', 'WONTFIX', 'SLOW']
# <platform> ::== [GPU] [CPU] [WIN] [LINUX] [MAC]
PLATFORM_NAMES = ['GPU', 'CPU', 'WIN', 'LINUX', 'MAC']
# <config> ::== RELEASE | DEBUG
CONFIG_NAMES = ['RELEASE', 'DEBUG']
# <EXPECTATION_NAMES> ::== \
#   [FAIL] [PASS] [CRASH] [TIMEOUT] [IMAGE] [TEXT] [IMAGE+TEXT]
EXPECTATION_NAMES = ['FAIL', 'PASS', 'CRASH',
                     'TIMEOUT', 'IMAGE', 'TEXT',
                     'IMAGE+TEXT']
ALL_TE_KEYWORDS = (DECISION_NAMES + PLATFORM_NAMES + CONFIG_NAMES +
                   EXPECTATION_NAMES)


class TestExpectations(object):
  """A class to model the content of test expectation file for analysis.

  The raw test expectation file can be found in
  |DEFAULT_TEST_EXPECTATION_LOCATION|.
  It is necessary to parse this file and store meaningful information for
  the analysis (joining with existing layout tests using a test name).
  Instance variable |all_test_expectation_info| is used.
  A test name such as 'media/video-source-type.html' is used for the key
  to store information. However, a test name can appear multiple times in
  the test expectation file. So, the map should keep all the occurrence
  information. For example, the current test expectation file has the following
  two entries:
  BUGWK58587 LINUX DEBUG GPU : media/video-zoom.html = IMAGE
  BUGCR86714 MAC GPU : media/video-zoom.html = CRASH IMAGE
  In this case, all_test_expectation_info['media/video-zoom.html'] will have
  a list with two elements, each of which is the map of the test expectation
  information.
  """

  def __init__(self, url=DEFAULT_TEST_EXPECTATION_LOCATION):
    """Read the test expectation file from the specified URL and parse it.

    All parsed information is stored into instance variable
    |all_test_expectation_info|, which is a dictionary mapping a string test
    name to a list of dictionaries containing test expectation entry
    information. An example of such dictionary:
      {'media/video-zoom.html': [{'LINUX': True, 'DEBUG': True ....},
                                 {'MAC': True, 'GPU': True     ....}]
    which is produced from the lines:
      BUGCR86714 MAC GPU : media/video-zoom.html = CRASH IMAGE
      BUGCR86714 LINUX DEBUG : media/video-zoom.html = IMAGE

    Args:
      url: A URL string for the test expectation file.

    Raises:
      NameError when the test expectation file cannot be retrieved from |url|.
    """
    self.all_test_expectation_info = {}
    resp = urllib2.urlopen(url)
    if resp.code != 200:
      raise NameError('Test expectation file does not exist in %s' % url)
    # Start parsing each line.
    comments = ''
    for line in resp.read().split('\n'):
      if line.startswith('//'):
        # Comments can be multiple lines.
        comments += line.replace('//', '')
      elif not line:
        comments = ''
      else:
        test_expectation_info = self.ParseLine(line, comments)
        testname = TestExpectations.ExtractTestOrDirectoryName(line)
        if not testname in self.all_test_expectation_info:
          self.all_test_expectation_info[testname] = []
        # This is a list for multiple entries.
        self.all_test_expectation_info[testname].append(test_expectation_info)

  @staticmethod
  def ExtractTestOrDirectoryName(line):
    """Extract either a test name or a directory name from each line.

    Please note the name in the test expectation entry can be test name or
    directory: Such examples are:
      BUGWK43668 SKIP : media/track/ = TIMEOUT

    Args:
      line: a line in the test expectation file.

    Returns:
      a test name or directory name string. Returns '' if no match.

    Raises:
      ValueError when there is no test name match.
    """
    # First try to find test name ending with .html.
    matches = re.search(r':\s+(\S+(.html|.svg))', line)
    # Next try to find directory name.
    if matches:
      return matches.group(1)
    matches = re.search(r':\s+(\S+)', line)
    if matches:
      return matches.group(1)
    else:
      raise ValueError('test or dictionary name cannot be found in the line')

  @staticmethod
  def ParseLine(line, comment_prefix):
    """Parse each line in test expectation and update test expectation info.

      This function checks for each entry from |ALL_TE_KEYWORDS| in the current
      line and stores it in the test expectation info map if found. Comment
      and bug information is also stored in the map.

    Args:
      line: a line in the test expectation file. For example,
          "BUGCR86714 MAC GPU : media/video-zoom.html = CRASH IMAGE"
      comment_prefix: comments from the test expectation file occurring just
          before the current line being parsed.

    Returns:
      a dictionary containing test expectation info, including comment and bug
          info.
    """
    test_expectation_info = {}
     # Store comments.
    inline_comments = ''
    if '//' in line:
      inline_comments = line[line.rindex('//') + 2:]
      # Remove the inline comments to avoid the case where keywords are in
      # inline comments.
      line = line[0:line.rindex('//')]
    for name in ALL_TE_KEYWORDS:
      if name in line:
        test_expectation_info[name] = True
    test_expectation_info['Comments'] = comment_prefix + inline_comments
    # Store bug informations.
    bugs = re.findall(r'BUG\w+', line)
    if bugs:
      test_expectation_info['Bugs'] = bugs
    return test_expectation_info
