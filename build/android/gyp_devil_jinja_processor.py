#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Special script for processing the devil config jinja file.

This script is needed because the PRODUCT_DIR is not known at gyp time. Many of
the jinja variables for the devil template are directories relative to the
PRODUCT_DIR. These paths cannot be computed at gyp time and need a separate
script.
"""

# TODO(mikecase): Delete this script once GYP is removed.

import argparse
import os
import subprocess
import sys

JINJA_TEMPLATE_SCRIPT = os.path.join(os.path.dirname(__file__),
                                    'gyp', 'jinja_template.py')

def DevilJinjaProcessor(args):

  gen_dir = os.path.join(args.product_dir, 'gen')

  rebased_android_sdk_root = os.path.relpath(args.android_sdk_root, gen_dir)
  rebased_product_dir = os.path.relpath(args.product_dir, gen_dir)

  variables = ['android_app_abi=%s' % args.android_abi,
               'android_sdk_root=%s' % rebased_android_sdk_root,
               'build_tools_version=%s' % args.build_tools_version,
               'output_dir=%s' % rebased_product_dir]

  output_file = os.path.join(gen_dir, 'devil_chromium.json')

  cmd = [JINJA_TEMPLATE_SCRIPT,
         '--inputs', args.input_file_path,
         '--output', output_file,
         '--variables', ' '.join(variables)]

  subprocess.call(cmd)

def main():
  parser = argparse.ArgumentParser()

  parser.add_argument('--android-abi', required=True,
                      help='Android app abi, e.g. armeabi-v7a.')
  parser.add_argument('--android-sdk-root', required=True,
                      help='Path to the android sdk root.')
  parser.add_argument('--build-tools-version', required=True,
                      help='Android build tools version.')
  parser.add_argument('--input-file-path', required=True,
                      help='Path to input template file.')
  parser.add_argument('--output-file-name', required=True,
                      help='Name of generated file. Will be put in gen/ '
                           'under product directory.')
  parser.add_argument('--product-dir', required=True,
                      help='Path to product directory.')

  args = parser.parse_args()
  DevilJinjaProcessor(args)

if __name__ == '__main__':
  sys.exit(main())
