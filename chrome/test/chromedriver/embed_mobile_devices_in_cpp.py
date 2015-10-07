#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Embeds standalone JavaScript snippets in C++ code.

The script requires the Source/devtools/front_end/emulated_devices/module.json
file from Blink that lists the known mobile devices to be passed in as the only
argument.  The list of known devices will be written to a C-style string to be
parsed with JSONReader.
"""

import json
import optparse
import re
import subprocess
import sys

import cpp_source


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--directory', type='string', default='.',
      help='Path to directory where the cc/h files should be created')
  options, args = parser.parse_args()

  devices = {}
  file_name = args[0]
  inside_list = False
  with open(file_name, 'r') as f:
    emulated_devices = json.load(f)
  extensions = emulated_devices['extensions']
  for extension in extensions:
    if extension['type'] == 'emulated-device':
      device = extension['device']
      devices[device['title']] = {
        'userAgent': device['user-agent'],
        'width': device['screen']['vertical']['width'],
        'height': device['screen']['vertical']['height'],
        'deviceScaleFactor': device['screen']['device-pixel-ratio'],
        'touch': 'touch' in device['capabilities'],
        'mobile': 'mobile' in device['capabilities'],
      }

  output_dir = 'chrome/test/chromedriver/chrome'
  cpp_source.WriteSource('mobile_device_list',
                         output_dir,
                         options.directory,
                         {'kMobileDevices': json.dumps(devices)})

  clang_format = ['clang-format', '-i']
  subprocess.Popen(clang_format + ['%s/mobile_device_list.cc' % output_dir])
  subprocess.Popen(clang_format + ['%s/mobile_device_list.h' % output_dir])


if __name__ == '__main__':
  sys.exit(main())
