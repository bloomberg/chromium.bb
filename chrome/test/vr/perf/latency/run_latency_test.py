# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script for automatically measuring motion-to-photon latency for VR.

Doing so requires two specialized pieces of hardware. The first is a Motopho,
which when used with a VR flicker app, finds the delay between movement and
the test device's screen updating in response to the movement. The second is
a set of servos, which physically moves the test device and Motopho during the
latency test.
"""

import android_webvr_latency_test

import argparse
import logging
import os
import sys

# TODO(bsheedy): See about having versioned copies of the flicker app
# instead of using personal github.
DEFAULT_FLICKER_APP_URL = ('https://weableandbob.github.io/Motopho/'
                           'flicker_apps/webvr/webvr-flicker-app-klaus.html?'
                           'polyfill=0\&canvasClickPresents=1')
DEFAULT_ADB_PATH = os.path.realpath('../../third_party/android_tools/sdk/'
                                    'platform-tools/adb')
# TODO(bsheedy): See about adding tool via DEPS instead of relying on it
# existing on the bot already
DEFAULT_MOTOPHO_PATH = os.path.join(os.path.expanduser('~'), 'motopho/Motopho')
DEFAULT_NUM_SAMPLES = 10
DEFAULT_RESULTS_FILE = 'results-chart.json'
DEFAULT_VRCORE_VERSION_FILE = 'vrcore_version.txt'


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
  parser.add_argument('--motopho-path',
                      type=os.path.realpath,
                      help='The absolute path to the directory with Motopho '
                           'scripts',
                      default=DEFAULT_MOTOPHO_PATH)
  parser.add_argument('--output-dir',
                      type=os.path.realpath,
                      help='The directory where the script\'s output files '
                           'will be saved')
  parser.add_argument('--platform',
                      help='The platform the test is being run on, either '
                           '"android" or "windows"')
  parser.add_argument('--results-file',
                      default=DEFAULT_RESULTS_FILE,
                      help='The name of the JSON file the results will be '
                           'saved to')
  parser.add_argument('--num-samples',
                      default=DEFAULT_NUM_SAMPLES,
                      help='The number of times to run the test before '
                           'the results are averaged')
  parser.add_argument('--url',
                      default=DEFAULT_FLICKER_APP_URL,
                      help='The URL of the flicker app to use')
  parser.add_argument('-v', '--verbose',
                      dest='verbose_count', default=0, action='count',
                      help='Verbose level (multiple times for more)')
  parser.add_argument('--vrcore-version-file',
                      default=DEFAULT_VRCORE_VERSION_FILE,
                      help='The name of the text file that the VrCore APK '
                           'version number will be saved to')
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
  latency_test = None
  if args.platform == 'android':
    latency_test = android_webvr_latency_test.AndroidWebVrLatencyTest(args)
  elif args.platform == 'win':
    raise NotImplementedError('WebVR not currently supported on Windows')
  else:
    raise RuntimeError('Given platform %s not recognized' % args.platform)
  latency_test.RunTest()


if __name__ == '__main__':
  sys.exit(main())
