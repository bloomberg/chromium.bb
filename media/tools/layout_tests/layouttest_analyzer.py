#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main functions for the Layout Test Analyzer module."""

import csv
from datetime import datetime
import optparse
import os
import time

from layouttests import LayoutTests
import layouttest_analyzer_helpers
from test_expectations import TestExpectations
from trend_graph import TrendGraph

# Predefined result directory.
DEFAULT_RESULT_DIR = 'result'
DEFAULT_ANNO_FILE = os.path.join('anno', 'anno.csv')
DEFAULT_GRAPH_FILE = os.path.join('graph', 'graph.html')
# Predefined result files for debug.
CUR_TIME_FOR_DEBUG = '2011-09-11-19'
CURRENT_RESULT_FILE_FOR_DEBUG = os.path.join(DEFAULT_RESULT_DIR,
                                             CUR_TIME_FOR_DEBUG)
PREV_TIME_FOR_DEBUG = '2011-09-11-18'

DEFAULT_TEST_GROUP_FILE = os.path.join('testname', 'media.csv')
DEFAULT_TEST_GROUP_NAME = 'media'


def parse_option():
    """Parse command-line options using OptionParser.

    Returns:
        an object containing all command-line option information.
    """
    option_parser = optparse.OptionParser()

    option_parser.add_option('-r', '--receiver-email-address',
                             dest='receiver_email_address',
                             help=('receiver\'s email adddress (defaults to '
                                   '%default'),
                             default='imasaki@chromium.org')
    option_parser.add_option('-g', '--debug-mode', dest='debug',
                             help=('Debug mode is used when you want to debug '
                                   'the analyzer by using local file rather '
                                   'than getting data from SVN. This shortens '
                                   'the debugging time (off by default)'),
                             action='store_true', default=False)
    option_parser.add_option('-t', '--trend-graph-location',
                             dest='trend_graph_location',
                             help=('Location of the bug trend file; '
                                   'file is expected to be in Google '
                                   'Visualization API trend-line format '
                                   '(defaults to %default)'),
                             default=DEFAULT_GRAPH_FILE)
    option_parser.add_option('-a', '--bug-anno-file-location',
                             dest='bug_annotation_file_location',
                             help=('Location of the bug annotation file; '
                                   'file is expected to be in CSV format '
                                   '(default to %default)'),
                             default=DEFAULT_ANNO_FILE)
    option_parser.add_option('-n', '--test-group-file-location',
                             dest='test_group_file_location',
                             help=('Location of the test group file; '
                                   'file is expected to be in CSV format '
                                   '(default to %default)'),
                             default=DEFAULT_TEST_GROUP_FILE)
    option_parser.add_option('-x', '--test-group-name',
                             dest='test_group_name',
                             help=('Name of test group '
                                   '(default to %default)'),
                             default=DEFAULT_TEST_GROUP_NAME)
    option_parser.add_option('-d', '--result-directory-location',
                             dest='result_directory_location',
                             help=('Name of result directory location '
                                   '(default to %default)'),
                             default=DEFAULT_RESULT_DIR)
    option_parser.add_option('-b', '--email-appended-text-file-location',
                             dest='email_appended_text_file_location',
                             help=('File location of the email appended text. '
                                   'The text is appended in the status email. '
                                   '(default to %default and no text is '
                                   'appended in that case.)'),
                             default=None)
    option_parser.add_option('-c', '--email-only-change-mode',
                             dest='email_only_change_mode',
                             help=('With this mode, email is sent out '
                                   'only when there is a change in the '
                                   'analyzer result compared to the previous '
                                   'result (off by default)'),
                             action='store_true', default=False)
    return option_parser.parse_args()[0]


def main():
  """A main function for the analyzer."""
  options = parse_option()
  start_time = datetime.now()

  # Do the main analysis.
  if not options.debug:
    layouttests = LayoutTests(csv_file_path=options.test_group_file_location)
    analyzer_result_map = layouttest_analyzer_helpers.AnalyzerResultMap(
        layouttests.JoinWithTestExpectation(TestExpectations()))
    (prev_time, prev_analyzer_result_map) = (
        layouttest_analyzer_helpers.FindLatestResult(
            options.result_directory_location))
  else:
    analyzer_result_map = layouttest_analyzer_helpers.AnalyzerResultMap.Load(
        CURRENT_RESULT_FILE_FOR_DEBUG)
    prev_time = PREV_TIME_FOR_DEBUG
    prev_analyzer_result_map = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(
            os.path.join(DEFAULT_RESULT_DIR, prev_time)))

  # Read bug annotations and generate an annotation map used for the status
  # email.
  anno_map = {}
  try:
    file_object = open(options.bug_annotation_file_location)
  except IOError:
    print 'cannot open annotation file %s. Skipping.' % (
        options.bug_annotation_file_location)
  else:
    data = csv.reader(file_object)
    for row in data:
      anno_map[row[0]] = row[1]
    file_object.close()

  appended_text_to_email = ''
  if options.email_appended_text_file_location:
    try:
      file_object = open(options.email_appended_text_file_location)
    except IOError:
      print 'cannot open email appended text file %s. Skipping.' % (
          options.email_appended_text_file_location)
    else:
      appended_text_to_email = ''.join(file_object.readlines())
      file_object.close()

  diff_map = analyzer_result_map.CompareToOtherResultMap(
      prev_analyzer_result_map)
  result_change = (any(diff_map['whole']) or any(diff_map['skip']) or
                      any(diff_map['nonskip']))
  # Email only when |email_only_change_mode| is False or there
  # is a change in the result compared to the last result.
  simple_rev_str = ''
  if not options.email_only_change_mode or result_change:
    prev_time_in_float = datetime.strptime(prev_time, '%Y-%m-%d-%H')
    prev_time_in_float = time.mktime(prev_time_in_float.timetuple())
    if options.debug:
      cur_time_in_float = datetime.strptime(CUR_TIME_FOR_DEBUG, '%Y-%m-%d-%H')
      cur_time_in_float = time.mktime(cur_time_in_float.timetuple())
    else:
      cur_time_in_float = time.time()
    (rev_str, simple_rev_str) = (
        layouttest_analyzer_helpers.GetRevisionString(prev_time_in_float,
                                                      cur_time_in_float,
                                                      diff_map))
    layouttest_analyzer_helpers.SendStatusEmail(
        prev_time, analyzer_result_map, diff_map, anno_map,
        options.receiver_email_address, options.test_group_name,
        appended_text_to_email, rev_str)
  if simple_rev_str:
    simple_rev_str = '\'' + simple_rev_str + '\''
  else:
    simple_rev_str = 'undefined'  # GViz uses undefined for NONE.
  if not options.debug:
    # Save the current result.
    date = start_time.strftime('%Y-%m-%d-%H')
    file_path = os.path.join(options.result_directory_location, date)
    analyzer_result_map.Save(file_path)

  if result_change:
    # Trend graph update (if specified in the command-line argument) when
    # there is change from the last result.
    # Currently, there are two graphs (graph1 is for 'whole', 'skip',
    # 'nonskip' and the graph2 is for 'passingrate'). Please refer to
    # graph/graph.html.
    # Sample JS annotation for graph1:
    #   [new Date(2011,8,12,10,41,32),224,undefined,'',52,undefined,undefined,
    #   12, 'test1,','<a href="http://t</a>,',],
    # This example lists 'whole' triple and 'skip' triple and
    # 'nonskip' triple. Each triple is (the number of tests that belong to
    # the test group, linked text, a link). The following code generates this
    # automatically based on rev_string etc.
    trend_graph = TrendGraph(options.trend_graph_location)
    datetime_string = start_time.strftime('%Y,%m,%d,%H,%M,%S')
    data_map = {}
    passingrate_anno = ''
    for test_group in ['whole', 'skip', 'nonskip']:
      anno = 'undefined'
      tests = analyzer_result_map.result_map[test_group].keys()
      if diff_map[test_group]:
        test_str = ''
        links = ''
        for i in [0, 1]:
          for (name, _) in diff_map[test_group][i]:
            test_str += name + ','
            # This is link to test HTML in WebKit SVN.
            links += ('<a href="http://trac.webkit.org/browser/trunk/'
                      'LayoutTests/%s">%s</a>,') % (name, name)
        if test_str:
          anno = '\'' + test_str + '\''
          # The annotation of passing rate is a union of all annotations.
          passingrate_anno += anno
        if test_group is 'whole':
          data_map[test_group] = (str(len(tests)), anno, '\'' + links + '\'')
        else:
          data_map[test_group] = (str(len(tests)), anno, simple_rev_str)
    if not passingrate_anno:
      passingrate_anno = 'undefined'
    data_map['passingrate'] = (
        str(analyzer_result_map.GetPassingRate()), passingrate_anno,
        simple_rev_str)
    trend_graph.Update(datetime_string, data_map)


if '__main__' == __name__:
  main()
