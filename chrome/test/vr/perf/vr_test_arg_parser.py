# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import logging
import os


DEFAULT_ADB_PATH = os.path.realpath('../../third_party/android_tools/sdk/'
                                    'platform-tools/adb')
DEFAULT_DURATION_SECONDS = 30
# TODO(bsheedy): See about adding tool via DEPS instead of relying on it
# existing on the bot already.
DEFAULT_MOTOPHO_PATH = os.path.join(os.path.expanduser('~'), 'motopho/Motopho')
DEFAULT_NUM_SAMPLES = 10
DEFAULT_RESULTS_FILE = 'results-chart.json'


class VrTestArgParser(argparse.ArgumentParser):
  def __init__(self):
    super(VrTestArgParser, self).__init__()
    self.AddCommonOptions()

  def AddCommonOptions(self):
    """Adds argument parsing options that are common to all VR perf tests."""
    self.add_argument('--adb-path',
                      type=os.path.realpath,
                      help='The absolute path to adb',
                      default=DEFAULT_ADB_PATH)
    self.add_argument('--additional-flags',
                      default=None,
                      help='A string containing any additional Chrome command '
                           'line flags to set for a test run')
    self.add_argument('--output-dir',
                      type=os.path.realpath,
                      help='The directory where the script\'s output files '
                           'will be saved')
    self.add_argument('--results-file',
                      default=DEFAULT_RESULTS_FILE,
                      help='The name of the JSON file the results will be '
                           'saved to')
    self.add_argument('--url',
                      action='append',
                      default=[],
                      dest='urls',
                      help='The URL of a WebVR app to use. Defaults to a set '
                           'of URLs with various CPU and GPU loads')
    self.add_argument('-v', '--verbose',
                      dest='verbose_count', default=0, action='count',
                      help='Verbose level (multiple times for more)')

  def AddFpsOptions(self):
    """Adds argument parsing options specific to VrCore FPS tests."""
    self.add_argument('--duration',
                      default=DEFAULT_DURATION_SECONDS,
                      type=int,
                      help='The duration spent collecting data from each URL')

  def AddLatencyOptions(self):
    """Adds argument parsing options specific to motopho latency tests."""
    self.add_argument('--motopho-path',
                      type=os.path.realpath,
                      help='The absolute path to the directory with Motopho '
                           'scripts',
                      default=DEFAULT_MOTOPHO_PATH)
    self.add_argument('--platform',
                      help='The platform the test is being run on, either '
                           '"android" or "windows"')
    self.add_argument('--num-samples',
                      default=DEFAULT_NUM_SAMPLES,
                      type=int,
                      help='The number of times to run the test before the '
                           'results are averaged')

  def ParseArgumentsAndSetLogLevel(self):
    """Parses the commandline arguments based on the options set.

    Also sets the log level based on the number of -v's input.
    Returns:
      The parsed args from an ArgumentParser object
    """
    (args, unknown_args) = self.parse_known_args()
    self.SetLogLevel(args.verbose_count)
    if unknown_args:
      self.error('Received unknown arguments: %s' % ' '.join(unknown_args))
    return args

  def SetLogLevel(self, verbose_count):
    """Sets the log level based on the command line arguments."""
    log_level = logging.WARNING
    if verbose_count == 1:
      log_level = logging.INFO
    elif verbose_count >= 2:
      log_level = logging.DEBUG
    logger = logging.getLogger()
    logger.setLevel(log_level)
