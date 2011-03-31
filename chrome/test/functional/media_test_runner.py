#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module to execute a subclass of MediaTastBase class.

This executes a media test class (a subclass of MediaTastBase class) with
different configuration (parameters) which are passed in the form of
environment variables (e.g., the number of runs). The location of the
subclass is passed as one of arguments. An example of invocation is
"./media_test_runner.py -p ./media_perf.py". In this example,
media_perf.py will be invoked using the default set of parameters.
The list of possible combinations of parameters are: T parameter
for media cache is set/non-set, Chrome flag is set/non-set, data element
in data source file (CSV file - its content is list form or its content is
in matrix form),
"""

import copy
import csv
import os
from optparse import OptionParser
import shlex
import sys
from subprocess import Popen

from media_test_matrix import MediaTestMatrix

EXTRA_NICKNAMES = ['nocache', 'cache']
# Disable/enable media_cache.
CHROME_FLAGS = ['--chrome-flags=\'--media-cache-size=1\'', '']
# T parameter is passed to player.html to disable/enable cache.
ADD_T_PARAMETERS = ['Y', 'N']
DEFAULT_PERF_PROG_NAME = 'media_perf.py'
DEFAULT_PLAYER_HTML_URL = 'DEFAULT'
DEFAULT_PLAYER_HTML_URL_NICKNAME = 'local'
PRINT_ONLY_TIME = 'Y'
REMOVE_FIRST_RESULT = 'N'
DEFAULT_NUMBER_OF_RUNS = 3
DEFAULT_MEASURE_INTERVALS = 3


def main():
  input_filename = os.path.join(os.pardir, 'data', 'media', 'dataset.csv')
  parser = OptionParser()
  # TODO(imasaki@chromium.org): add parameter verification.
  parser.add_option(
      '-i', '--input', dest='input_filename', default=input_filename,
      help='Data source file (file contents in list form) [defaults to "%s"]' %
      input_filename, metavar='FILE')
  parser.add_option(
      '-s', '--input_matrix', dest='input_matrix_filename',
      help='Data source file (file contents in matrix form)', metavar='FILE')
  parser.add_option('-t', '--input_matrix_testcase',
                    dest='input_matrix_testcase_name',
                    help='Run particular test in matrix')
  parser.add_option('-x', '--video_matrix_home_url',
                    default='',
                    dest='video_matrix_home_url',
                    help='Video Matrix home URL')
  parser.add_option('-p', '--perf_prog_name', dest='perf_prog_name',
                    default=DEFAULT_PERF_PROG_NAME,
                    help='Performance main program name [defaults to "%s"]' %
                         DEFAULT_PERF_PROG_NAME, metavar='FILE')
  parser.add_option('-b', '--player_html_url', dest='player_html_url',
                    default=DEFAULT_PLAYER_HTML_URL,
                    help='Player.html URL [defaults to "%s"] ' %
                         DEFAULT_PLAYER_HTML_URL, metavar='FILE')
  parser.add_option('-u', '--player_html_url_nickname',
                    dest='player_html_url_nickname',
                    default=DEFAULT_PLAYER_HTML_URL_NICKNAME,
                    help='Player.html Nickname [defaults to "%s"]' %
                         DEFAULT_PLAYER_HTML_URL_NICKNAME)
  parser.add_option('-n', '--number_of_runs', dest='number_of_runs',
                    default=DEFAULT_NUMBER_OF_RUNS,
                    help='The number of runs [defaults to "%d"]' %
                         DEFAULT_NUMBER_OF_RUNS)
  parser.add_option('-m', '--measure_intervals', dest='measure_intervals',
                    default=DEFAULT_MEASURE_INTERVALS,
                    help='Interval for measurement data [defaults to "%d"]' %
                         DEFAULT_MEASURE_INTERVALS)
  parser.add_option('-o', '--test-one-combination', dest='one_combination',
                    default=True, # Currently default is True
                                  # since we want to test only 1 combination.
                    help='Run only one parameter combination')
  options, args = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  test_data_list = []
  if options.input_matrix_filename is None:
    file = open(options.input_filename, 'rb')
    test_data_list = csv.reader(file)
    # First line contains headers that can be skipped.
    test_data_list.next()
  else:
    # Video_matrix_home_url requires "/" at the end.
    if not options.video_matrix_home_url.endswith('/'):
      options.video_matrix_home_url += '/'
    media_test_matrix = MediaTestMatrix()
    media_test_matrix.ReadData(options.input_matrix_filename)
    all_data_list = media_test_matrix.GenerateAllMediaInfosInCompactForm(
        True, options.video_matrix_home_url)
    if options.input_matrix_testcase_name is None:
      # Use all test cases.
      test_data_list = all_data_list
    else:
      # Choose particular test case.
      media_info = MediaTestMatrix.LookForMediaInfoInCompactFormByNickName(
          all_data_list, options.input_matrix_testcase_name)
      if media_info is not None:
        test_data_list.append(media_info)
  for tag, filename, nickname in test_data_list:
      for j in range(len(CHROME_FLAGS)):
        for k in range(len(ADD_T_PARAMETERS)):
           parent_envs = copy.deepcopy(os.environ)
           envs = {
             'HTML_TAG': tag,
             'MEDIA_FILENAME': filename,
             'MEDIA_FILENAME_NICKNAME': nickname,
             'PLAYER_HTML_URL': options.player_html_url,
             'PLAYER_HTML_URL_NICKNAME': options.player_html_url_nickname,
             'EXTRA_NICKNAME': EXTRA_NICKNAMES[j],
             'ADD_T_PARAMETER': ADD_T_PARAMETERS[k],
             'PRINT_ONLY_TIME': PRINT_ONLY_TIME,
             'N_RUNS': str(options.number_of_runs),
             'REMOVE_FIRST_RESULT': REMOVE_FIRST_RESULT,
             'MEASURE_INTERVALS': str(options.measure_intervals),
           }
           envs.update(parent_envs)
           cmd = [options.perf_prog_name, CHROME_FLAGS[j]]
           proc = Popen(cmd, env=envs, shell=True)
           proc.communicate()
           if options.one_combination:
             sys.exit(0)


if __name__ == '__main__':
  main()
