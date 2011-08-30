#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main functions for the Layout Test Analyzer module."""

import csv
from datetime import datetime
import optparse
import os
import pickle
import time


from layouttests import LayoutTests
import layouttest_analyzer_helpers
from test_expectations import TestExpectations
from trend_graph import TrendGraph

# Predefined result directory.
RESULT_DIR = 'result'
DEFAULT_ANNO_FILE = os.path.join('anno', 'anno.csv')
DEFAULT_GRAPH_FILE = os.path.join('graph', 'graph.html')
# Predefined result files for debug.
CURRENT_RESULT_FILE_FOR_DEBUG = os.path.join(RESULT_DIR, '2011-08-19-21')
PREV_TIME_FOR_DEBUG = '2011-08-19-11'

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
      layouttest_analyzer_helpers.FindLatestResult('result'))
  else:
    analyzer_result_map = layouttest_analyzer_helpers.AnalyzerResultMap.Load(
        CURRENT_RESULT_FILE_FOR_DEBUG)
    prev_time = PREV_TIME_FOR_DEBUG
    prev_analyzer_result_map = (
        layouttest_analyzer_helpers.AnalyzerResultMap.Load(
            os.path.join(RESULT_DIR, prev_time)))

  # Read bug annotations and generate an annotation map used for the status
  # email.
  anno_map = {}
  file_object = open(options.bug_annotation_file_location)
  data = csv.reader(file_object)
  for row in data:
    anno_map[row[0]] = row[1]
  file_object.close()

  layouttest_analyzer_helpers.SendStatusEmail(prev_time, analyzer_result_map,
                                              prev_analyzer_result_map,
                                              anno_map,
                                              options.receiver_email_address,
                                              options.test_group_name)
  if not options.debug:
    # Save the current result.
    date = start_time.strftime('%Y-%m-%d-%H')
    file_path = os.path.join(RESULT_DIR, date)
    analyzer_result_map.Save(file_path)

  # Trend graph update (if specified in the command-line argument).
  trend_graph = TrendGraph(options.trend_graph_location)
  datetime_string = start_time.strftime('%Y,%m,%d,%H,%M,%S')
  # TODO(imasaki): add correct title and text instead of 'undefined'.
  data_map = (
      {'whole': (str(len(analyzer_result_map.result_map['whole'].keys())),
                 'undefined', 'undefined'),
       'skip': (str(len(analyzer_result_map.result_map['skip'].keys())),
                'undefined', 'undefined'),
       'nonskip': (str(len(analyzer_result_map.result_map['nonskip'].keys())),
                   'undefined', 'undefined'),
       'passingrate': (str(analyzer_result_map.GetPassingRate()),
                       'undefined', 'undefined')})
  trend_graph.Update(datetime_string, data_map)


if '__main__' == __name__:
  main()
