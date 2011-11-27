#!/usr/bin/env python
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
import glob
import logging
import os
from optparse import OptionParser
import shlex
import sys
from subprocess import Popen

from media_test_env_names import MediaTestEnvNames
from media_test_matrix import MediaTestMatrix


def main():
  CHROME_FLAGS = {'disable_cache': '--media-cache-size=1',
                  # Please note track (caption) option is not implemented yet
                  # as of 6/8/2011.
                  'track': '--enable-video-track'}
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
  DEFAULT_SUITE_NAME = 'AV_PERF'
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
  parser.add_option('-c', '--disable_media_cache', dest='disable_media_cache',
                    default=False, help='Disable media cache',
                    action='store_true')
  parser.add_option('-z', '--test-one-video', dest='one_video',
                    default=False, help='Run only one video',
                    action='store_true')
  parser.add_option(
      '-w', '--test_scenario_input_filename',
      dest='test_scenario_input_filename',
      default='', help='Test scenario file (CSV form)', metavar='FILE')
  parser.add_option(
      '-q', '--test_scenario', dest='test_scenario',
      default='', help='Test scenario (action triples delimited by \'|\')')
  parser.add_option('-s', '--suite', dest='suite',
                    help='Suite file')
  parser.add_option('-e', '--media_file', dest='media_file',
                    default='',
                    help=('Media file to be played using player.html. '
                          'The relative path needs to be specified starting '
                          'from data/html/ directory. '
                          'The data should have the following format: '
                          'tag(video|audio)|filename|nickname|video_title'))
  parser.add_option('-a', '--reference_build', dest='reference_build',
                    help='Include reference build run', default=False,
                    action='store_true')
  parser.add_option('-k', '--reference_build_dir', dest='reference_build_dir',
                    help=('A absolute path to the directory that contains'
                          'binaries of reference build.'))
  parser.add_option('-v', '--verbose', dest='verbose', help='Verbose mode.',
                    default=False, action='store_true')
  parser.add_option('-j', '--track', dest='track',
                    help=('Run track test (binary should be downloaded'
                          ' from http://www.annacavender.com/track/'
                          ' and put into reference_build_dir).'),
                    default=False, action='store_true')
  parser.add_option('-g', '--track-file', dest='track_file',
                    help=('Track file in vtt format (binary should be'
                          ' downloaded from http://www.annacavender.com/track/'
                          ' and put into reference_build_dir).'))
  parser.add_option('-y', '--track-file_dir', dest='track_file_dir',
                    help=('A directory that contains vtt format files.'))
  parser.add_option('-d', '--num-extra-players',
                    dest='number_of_extra_players',
                    help=('The number of extra players for '
                          'stress testing using the same media file.'))
  parser.add_option('-l', '--jerky-tool-location',
                    dest='jerky_tool_location',
                    help='The location of the jerky tool binary.')
  parser.add_option('--jo', '--jerky-tool-output-directory',
                    dest='jerky_tool_output_directory',
                    help='The output directory of the jerky tool.')
  parser.add_option('--jb', '--jerky-tool-baseline-directory',
                    dest='jerky_tool_baseline_directory',
                    help='The baseline directory of the jerky tool.')
  options, args = parser.parse_args()
  if args:
    parser.print_help()
    sys.exit(1)

  test_data_list = []
  if options.media_file:
    opts = options.media_file.split('|')
    # The media file should have the following format:
    # tag(video|audio)|filename|nickname|video_title.
    if len(opts) != 4:
      logging.error('--media_file option argument does not have correct'
                    'format. The correct format is tag(video|audio)'
                    '|filename|nickname|video_title')
      sys.exit(1)
    test_data_list.append(opts)
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
  track_files = ['']
  if any([options.track_file, options.track, options.track_file_dir]):
    # TODO(imasaki@chromium.org): change here after track functionality is
    # available on Chrome. Currently, track patch is still under development.
    # So, I need to download the binary from
    # http://www.annacavender.com/track/ and use it for testing.
    # I temporarily use reference build mechanism.
    reference_build_list = [True]
    if options.track_file_dir:
      track_files_orig = (
          glob.glob(os.path.join(options.track_file_dir, '*.vtt')))
      track_files = []
      for tf in track_files_orig:
        # The location should be relative path from HTML files.
        # So it needs to remove data and media from the path.
        track_files.append(tf.replace(os.path.join('data', 'media'), ''))
      if not track_files:
        logging.warning('No track files in %s', options.track_file_dir)
    if options.track_file:
      track_files = [options.track_file]
  # This is a loop for iterating through all videos defined above (list
  # or matrix). Each video has associated tag and nickname for display
  # purpose.
  for tag, filename, nickname, title in test_data_list:
    for track_file in track_files:
      for reference_build in reference_build_list:
        parent_envs = copy.deepcopy(os.environ)
        if options.input_matrix_filename is None:
          par_filename = os.path.join(os.pardir, filename)
        else:
          par_filename = filename
        envs = {
          MediaTestEnvNames.MEDIA_TAG_ENV_NAME: tag,
          MediaTestEnvNames.MEDIA_FILENAME_ENV_NAME: par_filename,
          MediaTestEnvNames.MEDIA_FILENAME_NICKNAME_ENV_NAME: nickname,
          MediaTestEnvNames.PLAYER_HTML_URL_ENV_NAME:
            options.player_html_url,
          MediaTestEnvNames.PLAYER_HTML_URL_NICKNAME_ENV_NAME:
            options.player_html_url_nickname,
          MediaTestEnvNames.N_RUNS_ENV_NAME: str(options.number_of_runs),
          MediaTestEnvNames.MEASURE_INTERVAL_ENV_NAME:
            str(options.measure_intervals),
          MediaTestEnvNames.TEST_SCENARIO_FILE_ENV_NAME:
            options.test_scenario_input_filename,
          MediaTestEnvNames.TEST_SCENARIO_ENV_NAME:
            options.test_scenario,
        }
        # Boolean variables and their related variables.
        if options.disable_media_cache:
          # The 't' parameter is passed to player.html to disable/enable
          # the media cache (refer to data/media/html/player.js).
          envs[MediaTestEnvNames.ADD_T_PARAMETER_ENV_NAME] = str(
              options.disable_media_cache)
        if reference_build:
          envs[MediaTestEnvNames.REFERENCE_BUILD_ENV_NAME] = str(
              reference_build)
        if REMOVE_FIRST_RESULT:
          envs[MediaTestEnvNames.REMOVE_FIRST_RESULT_ENV_NAME] = str(
              REMOVE_FIRST_RESULT)
        if options.reference_build_dir:
          envs[MediaTestEnvNames.REFERENCE_BUILD_DIR_ENV_NAME] = (
              options.reference_build_dir)
        if track_file:
          envs[MediaTestEnvNames.TRACK_FILE_ENV_NAME] = track_file
        if options.number_of_extra_players:
          envs[MediaTestEnvNames.N_EXTRA_PLAYERS_ENV_NAME] = (
              options.number_of_extra_players)
        if options.jerky_tool_location:
          envs[MediaTestEnvNames.JERKY_TOOL_BINARY_LOCATION_ENV_NAME] = (
              options.jerky_tool_location)
        if options.jerky_tool_output_directory:
          envs[MediaTestEnvNames.JERKY_TOOL_OUTPUT_DIR_ENV_NAME] = (
              options.jerky_tool_output_directory)
        if options.jerky_tool_baseline_directory:
          envs[MediaTestEnvNames.JERKY_TOOL_BASELINE_DIR_ENV_NAME] = (
              options.jerky_tool_baseline_directory)
        envs.update(parent_envs)
        if options.suite is None and options.test_prog_name is not None:
          # Suite is not used - run test program directly.
          test_prog_name = options.test_prog_name
          suite_string = ''
        else:
          # Suite is used.
          # The test script names are in the PYAUTO_TESTS file.
          test_prog_name = pyauto_functional_script_name
          if options.suite is None:
            suite_name = DEFAULT_SUITE_NAME
          else:
            suite_name = options.suite
          suite_string = ' --suite=%s' % suite_name
        test_prog_name = sys.executable + ' ' + test_prog_name
        chrome_flag = ''
        if options.disable_media_cache:
          chrome_flag += CHROME_FLAGS['disable_cache']
        if options.track_file:
          if options.disable_media_cache:
            chrome_flag += ' '
          chrome_flag += CHROME_FLAGS['track']
        if chrome_flag:
          chrome_flag = '--chrome-flags=\'%s\'' % chrome_flag
        cmd = test_prog_name + suite_string + ' ' + chrome_flag
        if options.verbose:
          cmd += ' -v'
        proc = Popen(cmd, env=envs, shell=True)
        proc.communicate()

    if options.one_video:
      break


if __name__ == '__main__':
  main()
