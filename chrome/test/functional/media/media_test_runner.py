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

from media_test_env_names import MediaTestEnvNames
from media_test_matrix import MediaTestMatrix


def main():
  EXTRA_NICKNAMES = ['nocache', 'cache']
  # Disable/enable media_cache.
  CHROME_FLAGS = ['--chrome-flags=\'--media-cache-size=1\'', '']
  # The 't' parameter is passed to player.html to disable/enable the media
  # cache (refer to data/media/html/player.js).
  ADD_T_PARAMETERS = [False, True]
  # Player.html should contain all the HTML and Javascript that is
  # necessary to run these tests.
  DEFAULT_PLAYER_HTML_URL = 'DEFAULT'
  DEFAULT_PLAYER_HTML_URL_NICKNAME = 'local'
  # Default base url nickname used to display the result in case it is not
  # specified by the environment variable.
  DEFAULT_PLAYER_HTML_URL_NICKNAME = 'local'
  REMOVE_FIRST_RESULT = True
  # The number of runs for each test. This is used to compute average values
  # from among all runs.
  DEFAULT_NUMBER_OF_RUNS = 3
  # The interval between measurement calls.
  DEFAULT_MEASURE_INTERVALS = 3
  DEFAULT_SUITE_NAME = 'MEDIA_TESTS'
  # This script is used to run the PYAUTO suite.
  pyauto_functional_script_name = os.path.join(os.path.dirname(__file__),
                                               'pyauto_media.py')
  # This defines default file name for CSV file.
  default_input_filename = os.path.join(os.pardir, 'data', 'media', 'csv',
                                        'media_list_data.csv')
  parser = OptionParser()
  # TODO(imasaki@chromium.org): add parameter verification.
  parser.add_option(
      '-i', '--input', dest='input_filename', default=default_input_filename,
      help='Data source file (file contents in list form) [defaults to "%s"]' %
      default_input_filename, metavar='FILE')
  parser.add_option(
      '-x', '--input_matrix', dest='input_matrix_filename',
      help='Data source file (file contents in matrix form)', metavar='FILE')
  parser.add_option('-t', '--input_matrix_testcase',
                    dest='input_matrix_testcase_name',
                    help='Run particular test in matrix')
  parser.add_option('-r', '--video_matrix_home_url',
                    default='',
                    dest='video_matrix_home_url',
                    help='Video Matrix home URL')
  parser.add_option('-p', '--test_prog_name', dest='test_prog_name',
                    help='Test main program name (not using suite)',
                    metavar='FILE')
  parser.add_option('-b', '--player_html_url', dest='player_html_url',
                    default='None',
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
  parser.add_option(
      '-w', '--test_scenario_input_filename',
      dest='test_scenario_input_filename',
      default='', help='Test scenario file (CSV form)', metavar='FILE')
  parser.add_option(
      '-c', '--test_scenario', dest='test_scenario',
      default='', help='Test scenario (action triples delimited by \'|\')')
  parser.add_option('-s', '--suite', dest='suite',
                    help='Suite file')
  parser.add_option('-e', '--media_file', dest='media_file',
                    default='',
                    help=('Media file to be played using player.html. '
                          'The relative path needs to be specified starting '
                          'from data/html/ directory.'))
  parser.add_option('-a', '--reference_build', dest='reference_build',
                    help='Include reference build run', default=False,
                    action='store_true')
  parser.add_option('-k', '--reference_build_dir', dest='reference_build_dir',
                    help=('A absolute path to the directory that contains'
                          'binaries of reference build.'))
  parser.add_option('-v', '--verbose', dest='verbose', help='Verbose mode.',
                    default=False, action='store_true')

  options, args = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  test_data_list = []
  if options.media_file:
    test_data_list.append(['video', options.media_file, options.media_file])
  elif options.input_matrix_filename is None:
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
      # Choose particular video.
      media_info = MediaTestMatrix.LookForMediaInfoInCompactFormByNickName(
          all_data_list, options.input_matrix_testcase_name)
      if media_info is not None:
        test_data_list.append(media_info)

  # Determine whether we need to repeat a test using a reference build.
  # The default is not to include a test using a reference build.
  if options.reference_build:
    reference_build_list = [False, True]
  else:
    reference_build_list = [False]
  # This is a loop for iterating through all videos defined above (list
  # or matrix). Each video has associated tag and nickname for display
  # purpose.
  for tag, filename, nickname in test_data_list:
      # This inner loop iterates twice. The first iteration of the loop
      # disables the media cache, and the second iteration enables the media
      # cache.  Other parameters remain the same on both loop iterations.
      # There are two ways to disable the media cache: setting Chrome option
      # to --media-cache-size=1 or adding t parameter in query parameter of
      # URL in which player.js (data/media/html/player.js) disables the
      # media cache). We are doing both here. Please note the length of
      # CHROME_FLAGS and ADD_T_PARAMETERS should be the same.
      for j in range(len(CHROME_FLAGS)):
        for reference_build in reference_build_list:
          parent_envs = copy.deepcopy(os.environ)
          if options.input_matrix_filename is None:
            par_filename = os.path.join(os.pardir, filename)
          envs = {
            MediaTestEnvNames.MEDIA_TAG_ENV_NAME: tag,
            MediaTestEnvNames.MEDIA_FILENAME_ENV_NAME: par_filename,
            MediaTestEnvNames.MEDIA_FILENAME_NICKNAME_ENV_NAME: nickname,
            MediaTestEnvNames.PLAYER_HTML_URL_ENV_NAME:
              options.player_html_url,
            MediaTestEnvNames.PLAYER_HTML_URL_NICKNAME_ENV_NAME:
              options.player_html_url_nickname,
            MediaTestEnvNames.EXTRA_NICKNAME_ENV_NAME:
              EXTRA_NICKNAMES[j],
            # Enables or disables the media cache.
            # (refer to data/media/html/player.js)
            MediaTestEnvNames.N_RUNS_ENV_NAME: str(options.number_of_runs),
            MediaTestEnvNames.MEASURE_INTERVAL_ENV_NAME:
              str(options.measure_intervals),
            MediaTestEnvNames.TEST_SCENARIO_FILE_ENV_NAME:
              options.test_scenario_input_filename,
            MediaTestEnvNames.TEST_SCENARIO_ENV_NAME:
              options.test_scenario,
          }
          # Boolean variables and their related variables.
          if ADD_T_PARAMETERS[j]:
            envs[MediaTestEnvNames.ADD_T_PARAMETER_ENV_NAME] = str(
                ADD_T_PARAMETERS[j])
          if reference_build:
            envs[MediaTestEnvNames.REFERENCE_BUILD_ENV_NAME] = str(
                reference_build)
          if REMOVE_FIRST_RESULT:
            envs[MediaTestEnvNames.REMOVE_FIRST_RESULT_ENV_NAME] = str(
                REMOVE_FIRST_RESULT)
          if options.reference_build_dir:
            envs[MediaTestEnvNames.REFERENCE_BUILD_DIR_ENV_NAME] = (
                options.reference_build_dir)
          envs.update(parent_envs)
          if options.suite is None and options.test_prog_name is not None:
            # Suite is not used - run test program directly.
            test_prog_name = options.test_prog_name
            suite_string = ''
          else:
            # Suite is used.
            # The test script names are in the PYAUTO_TEST file.
            test_prog_name = pyauto_functional_script_name
            if options.suite is None:
              suite_name = DEFAULT_SUITE_NAME
            else:
              suite_name = options.suite
            suite_string = ' --suite=%s' % suite_name
          test_prog_name = sys.executable + ' ' + test_prog_name
          cmd = test_prog_name + suite_string + ' ' + CHROME_FLAGS[j]
          if options.verbose:
            cmd += ' -v'
          proc = Popen(cmd, env=envs, shell=True)
          proc.communicate()
        if options.one_combination:
          sys.exit(0)


if __name__ == '__main__':
  main()
