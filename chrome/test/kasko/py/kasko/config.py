#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Command-line configuration utilities."""

import logging
import os
import optparse

from kasko.process import ImportSelenium


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

  return option_parser


def ParseCommandLine(option_parser=None):
  if not option_parser:
    option_parser = GenerateOptionParser()

  options, args = option_parser.parse_args()
  if args:
    option_parser.error('Unexpected arguments: %s' % args)

  # Validate chrome.exe exists.
  if not os.path.isfile(options.chrome):
    option_parser.error('chrome.exe not found')

  # Use default chromedriver.exe if necessary, and validate it exists.
  if not options.chromedriver:
    options.chromedriver = os.path.join(os.path.dirname(options.chrome),
                                        'chromedriver.exe')
  if not os.path.isfile(options.chromedriver):
    option_parser.error('chromedriver.exe not found')

  # If specified, ensure the webdriver parameters is a directory.
  if options.webdriver and not os.path.isdir(options.webdriver):
    option_parser.error('Invalid webdriver directory.')

  # Configure logging.
  logging.basicConfig(level=options.log_level)

  _LOGGER.debug('Using chrome path: %s', options.chrome)
  _LOGGER.debug('Using chromedriver path: %s', options.chromedriver)
  _LOGGER.debug('Using webdriver path: %s', options.webdriver)

  # Import webdriver and selenium.
  ImportSelenium(options.webdriver)

  return options
