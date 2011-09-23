#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Main function to run the layout test analyzer.

The purpose of this script is to run the layout test analyzer for various
teams based on the run configuration file in CSV format. The CSV file is based
on https://sites.google.com/a/chromium.org/dev/developers/testing/
webkit-layout-tests/layout-test-stats-1.
"""

import csv
import optparse
import os
import shutil
from subprocess import Popen
import sys

DEFAULT_RUNNER_CONFIG_FILE = os.path.join('runner_config',
                                          'runner_config.csv')

# Predefined result/graph directory.
DEFAULT_RESULT_DIR = 'result'
DEFAULT_GRAPH_DIR = 'graph'
DEFAULT_ANNO_DIR = 'anno'


def ParseOption():
  """Parse command-line options using OptionParser.

  Returns:
      an object containing all command-line option information.
  """
  option_parser = optparse.OptionParser()

  option_parser.add_option('-c', '--runner-config-file-location',
                           dest='runner_config_file_location',
                           help=('Location of the bug annotation file; '
                                 'file is expected to be in CSV format '
                                 '(default to %default)'),
                           default=DEFAULT_RUNNER_CONFIG_FILE)
  option_parser.add_option('-x', '--test-group-name',
                           dest='test_group_name',
                           help='A name of test group.')
  option_parser.add_option('-d', '--result-directory-location',
                           dest='result_directory_location',
                           help=('Name of result directory location '
                                 '(default to %default)'),
                           default=DEFAULT_RESULT_DIR)
  option_parser.add_option('-p', '--graph-directory-location',
                           dest='graph_directory_location',
                           help=('Name of graph directory location '
                                 '(default to %default)'),
                           default=DEFAULT_GRAPH_DIR)
  option_parser.add_option('-a', '--anno-directory-location',
                           dest='annotation_directory_location',
                           help=('Name of annotation directory location; '
                                 'each annotation file should be the same '
                                 'as test group name with replacement of "/"'
                                 'with "_" (default to %default)'),
                           default=DEFAULT_ANNO_DIR)
  option_parser.add_option('-b', '--email-appended-text-file-location',
                           dest='email_appended_text_file_location',
                           help=('File location of the email appended text. '
                                 'The text is appended in the status email. '
                                 '(default to %default and no text is '
                                 'appended in that case.)'),
                           default=None)
  return option_parser.parse_args()[0]


def GenerateDashboardHTMLFile(file_name, test_group_list):
  """Generate dashboard HTML file.

  Currently, it is simple table that shows all the analyzer results.

  Args:
    file_name: the file name of the dashboard.
    test_group_list: a list of test group names such as 'media' or 'composite'.
  """
  file_object = open(file_name, 'wb')
  file_object.write('<table border="1">')
  file_object.write('<tr><th>test group</th>')
  file_object.write('<th>#Tests</th>')
  file_object.write('<th>#Skipped Tests</th>')
  file_object.write('<th>#Non-Skipped Failing Tests</th>')
  file_object.write('<th>Passing Rate</td></tr>')
  for test_group in test_group_list:
    file_object.write('<tr>\n')
    file_object.write('<td>' + test_group + '</td>\n')
    file_object.write('</tr>')
  file_object.write('</table>')
  file_object.close()


def main():
  """A main function for the analyzer runner."""
  options = ParseOption()
  run_config_map = {}
  try:
    file_object = open(options.runner_config_file_location)
  except IOError:
    print 'cannot open runner configuration file %s. Exiting.' % (
        options.runner_config_file_location)
    sys.exit()
  data = csv.reader(file_object)
  # Skip the first row since it is a comment/header line.
  data.next()
  for row in data:
    run_config_map[row[0]] = (row[1], row[2])
  file_object.close()
  if options.test_group_name:
    test_group_list = [options.test_group_name]
  else:
    test_group_list = run_config_map.keys()
  dashboard_file_location = os.path.join(options.graph_directory_location,
                                         'index.html')
  if not os.path.exists(dashboard_file_location):
    GenerateDashboardHTMLFile(dashboard_file_location, test_group_list)
  for test_group in test_group_list:
    # Prepare the result if it does not exist.
    # The directory name should be changed to avoid collision
    # with the file separator.
    test_group_name_for_data = test_group.replace('/', '_')
    result_dir = os.path.join(options.result_directory_location,
                              test_group_name_for_data)
    if not os.path.exists(result_dir):
      os.mkdir(result_dir)
    graph_file = os.path.join(options.graph_directory_location,
                              test_group_name_for_data + '.html')
    if not os.path.exists(graph_file):
      # Copy the template file.
      shutil.copy(os.path.join('graph', 'graph.html'),
                  graph_file)
      os.chmod(graph_file, 0744)
    anno_file = os.path.join(options.annotation_directory_location,
                             test_group_name_for_data + '.csv')
    cmd = ('python layouttest_analyzer.py -x %s -n %s -r %s -d %s -t %s'
           ' -q %s -a %s -c') % (
               test_group, run_config_map[test_group][0],
               run_config_map[test_group][1], result_dir, graph_file,
               dashboard_file_location, anno_file)
    if options.email_appended_text_file_location:
      cmd += ' -b ' + options.email_appended_text_file_location
    print 'Running ' + cmd
    proc = Popen(cmd, shell=True)
    proc.communicate()


if '__main__' == __name__:
  main()
