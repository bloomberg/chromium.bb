#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command-line configuration utilities."""

import logging
import os
import optparse
import sys

from kasko.process import ImportSelenium
from kasko.json_logging_handler import JSONLoggingHandler


_LOGGER = logging.getLogger(os.path.basename(__file__))


def GenerateOptionParser():
  """Generates a default OptionParser."""

  """Parses the command-line and returns an options structure."""
  self_dir = os.path.dirname(__file__)
  src_dir = os.path.abspath(os.path.join(self_dir, '..', '..', '..', '..',
                                         '..'))

  option_parser = optparse.OptionParser()
  option_parser.add_option('--chrome', dest='chrome', type='string',
      default=os.path.join(src_dir, 'out', 'Release', 'chrome.exe'),
      help='Path to chrome.exe. Defaults to $SRC/out/Release/chrome.exe')
  option_parser.add_option('--chromedriver', dest='chromedriver',
      type='string', help='Path to the chromedriver.exe. By default will look '
      'alongside chrome.exe.')
  option_parser.add_option('--keep-temp-dirs', action='store_true',
      default=False, help='Prevents temporary directories from being deleted.')
  option_parser.add_option('--quiet', dest='log_level', action='store_const',
      default=logging.INFO, const=logging.ERROR,
      help='Disables all output except for errors.')
  option_parser.add_option('--user-data-dir', dest='user_data_dir',
      type='string', help='User data directory to use. Defaults to using a '
      'temporary one.')
  option_parser.add_option('--verbose', dest='log_level', action='store_const',
      default=logging.INFO, const=logging.DEBUG,
      help='Enables verbose logging.')
  option_parser.add_option('--webdriver', type='string',
      default=os.path.join(src_dir, 'third_party', 'webdriver', 'pylib'),
      help='Specifies the directory where the python installation of webdriver '
           '(selenium) can be found. Specify an empty string to use the system '
           'installation. Defaults to $SRC/third_party/webdriver/pylib')
  option_parser.add_option('--log-to-json', type='string',
      help='The path to the JSON file that should be used for the logging. '
           'Defaults to logging directly to the console.')

  return option_parser


def LogParserError(message, parser, log_to_json):
  """Log a parsing error on the command-line.

  This logs the error via the logger if the '--log-to-json' flag has been
  passed to the logger.
  """
  if log_to_json:
    _LOGGER.error(message)
    sys.exit(1)
  else:
    parser.error(message)


def ParseCommandLine(option_parser=None):
  if not option_parser:
    option_parser = GenerateOptionParser()

  options, args = option_parser.parse_args()

  # Configure logging.
  logging.basicConfig(level=options.log_level)

  if options.log_to_json:
    logging.getLogger().addHandler(JSONLoggingHandler(options.log_to_json))

  if args:
    return LogParserError('Unexpected arguments: %s' % args,
                          option_parser, options.log_to_json)

  # Validate chrome.exe exists.
  if not os.path.isfile(options.chrome):
    return LogParserError('chrome.exe not found',
                          option_parser, options.log_to_json)

  # Use default chromedriver.exe if necessary, and validate it exists.
  if not options.chromedriver:
    options.chromedriver = os.path.join(os.path.dirname(options.chrome),
                                        'chromedriver.exe')
  if not os.path.isfile(options.chromedriver):
    return LogParserError('chromedriver.exe not found',
                          option_parser, options.log_to_json)

  # If specified, ensure the webdriver parameters is a directory.
  if options.webdriver and not os.path.isdir(options.webdriver):
    return LogParserError('Invalid webdriver directory.',
                          option_parser, options.log_to_json)

  _LOGGER.debug('Using chrome path: %s', options.chrome)
  _LOGGER.debug('Using chromedriver path: %s', options.chromedriver)
  _LOGGER.debug('Using webdriver path: %s', options.webdriver)

  # Import webdriver and selenium.
  ImportSelenium(options.webdriver)

  return options
