# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for automatically measuring FPS for VR via VrCore perf logging.

Android only.
VrCore has a developer option to log performance data to logcat. This test
visits various WebVR URLs and collects the FPS data reported by VrCore.
"""

# Needs to be imported first in order to add the parent directory to path.
import vrcore_fps_test_config
import vrcore_fps_test

import argparse
import logging
import os

DEFAULT_ADB_PATH = os.path.realpath('../../third_party/android_tools/sdk/'
                                    'platform-tools/adb')
DEFAULT_DURATION_SECONDS = 30
DEFAULT_RESULTS_FILE = 'results-chart.json'


# TODO(bsheedy): Move common arg parsing code to shared file.
def GetParsedArgs():
  """Parses the command line arguments passed to the script.

  Fails if any unknown arguments are present.

  Returns:
    An object containing all known, parsed arguments.
  """
  parser = argparse.ArgumentParser()
  parser.add_argument('--adb-path',
                      type=os.path.realpath,
                      help='The absolute path to adb',
                      default=DEFAULT_ADB_PATH)
  parser.add_argument('--duration',
                      default=DEFAULT_DURATION_SECONDS,
                      type=int,
                      help='The duration spent collecting data from each URL')
  parser.add_argument('--output-dir',
                      type=os.path.realpath,
                      help='The directory where the script\'s output files '
                           'will be saved')
  parser.add_argument('--results-file',
                      default=DEFAULT_RESULTS_FILE,
                      help='The name of the JSON file the results will be '
                           'saved to')
  parser.add_argument('--url',
                      action='append',
                      default=[],
                      dest='urls',
                      help='The URL of a WebVR app to use. Defaults to a '
                           'set of URLs with various CPU and GPU loads')
  parser.add_argument('-v', '--verbose',
                      dest='verbose_count', default=0, action='count',
                      help='Verbose level (multiple times for more)')
  (args, unknown_args) = parser.parse_known_args()
  SetLogLevel(args.verbose_count)
  if unknown_args:
    parser.error('Received unknown arguments: %s' % ' '.join(unknown_args))
  return args


def SetLogLevel(verbose_count):
  """Sets the log level based on the command line arguments."""
  log_level = logging.WARNING
  if verbose_count == 1:
    log_level = logging.INFO
  elif verbose_count >= 2:
    log_level = logging.DEBUG
  logger = logging.getLogger()
  logger.setLevel(log_level)


def main():
  args = GetParsedArgs()
  test = vrcore_fps_test.VrCoreFpsTest(args)
  test.RunTests()


if __name__ == '__main__':
  main()
