#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the devtools/front_end/toolbox/OverridesUI.js file from
WebKit that lists the known mobile devices to be passed in as the only argument.
The list of known devices will be written to a C-style string to be parsed with
JSONReader.
"""

import optparse
import os
import re
import subprocess
import sys

import cpp_source


def quotizeKeys(s, keys):
  """Returns the string s with each instance of each key wrapped in quotes.

  Args:
    s: a string containing keys that need to be wrapped in quotes.
    keys: an iterable of keys to be wrapped in quotes in the string.
  """
  for key in keys:
    s = re.sub('%s: ' % key, '"%s": ' % key, s)
  return s


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
        if 'WebInspector.OverridesUI._phones = [' in line:
          inside_list = True
        if 'WebInspector.OverridesUI._tablets = [' in line:
          devices += ','
          inside_list = True
      else:
        if line.strip() == '];':
          inside_list = False
          continue
        devices += line.strip()

  output_dir = 'chrome/test/chromedriver/chrome'
  devices += ']'
  devices = quotizeKeys(devices,
                        ['title', 'width', 'height', 'deviceScaleFactor',
                         'userAgent', 'touch', 'mobile'])
  cpp_source.WriteSource('mobile_device_list',
                         output_dir,
                         options.directory, {'kMobileDevices': devices})

  clang_format = ['clang-format', '-i']
  subprocess.Popen(clang_format + ['%s/mobile_device_list.cc' % output_dir])
  subprocess.Popen(clang_format + ['%s/mobile_device_list.h' % output_dir])


if __name__ == '__main__':
  sys.exit(main())
