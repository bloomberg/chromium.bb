#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the OverridesView file from WebKit that lists the known
mobile devices to be passed in as the only argument. The list of known devices
will be written to a C-style string to be parsed with JSONReader.
"""

import optparse
import os
import sys

import cpp_source


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h files should be created')
  options, args = parser.parse_args()

  devices = '['
  file_name = args[0]
  inside_list = False
  with open(file_name, 'r') as f:
    for line in f:
      if not inside_list:
        if 'WebInspector.OverridesUI._phones = [' in line or \
           'WebInspector.OverridesUI._tablets = [' in line:
          inside_list = True
      else:
        if line.strip() == '];':
          inside_list = False
          continue
        devices += line.strip()

  devices += ']'
  cpp_source.WriteSource('mobile_device_list',
                         'chrome/test/chromedriver/chrome',
                         options.directory, {'kMobileDevices': devices})


if __name__ == '__main__':
  sys.exit(main())
