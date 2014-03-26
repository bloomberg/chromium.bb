#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signs and zipaligns APK.

"""

import optparse
import os
import shutil
import sys
import tempfile

from util import build_utils

def SignApk(key_path, key_name, key_passwd, unsigned_path, signed_path):
  shutil.copy(unsigned_path, signed_path)
  sign_cmd = [
      'jarsigner',
      '-sigalg', 'MD5withRSA',
      '-digestalg', 'SHA1',
      '-keystore', key_path,
      '-storepass', key_passwd,
      signed_path,
      key_name,
    ]
  build_utils.CheckOutput(sign_cmd)


def AlignApk(android_sdk_root, unaligned_path, final_path):
  align_cmd = [
      os.path.join(android_sdk_root, 'tools', 'zipalign'),
      '-f', '4',  # 4 bytes
      unaligned_path,
      final_path,
      ]
  build_utils.CheckOutput(align_cmd)


def main():
  parser = optparse.OptionParser()

  parser.add_option('--android-sdk-root', help='Android sdk root directory.')
  parser.add_option('--unsigned-apk-path', help='Path to input unsigned APK.')
  parser.add_option('--final-apk-path',
      help='Path to output signed and aligned APK.')
  parser.add_option('--key-path', help='Path to keystore for signing.')
  parser.add_option('--key-passwd', help='Keystore password')
  parser.add_option('--key-name', help='Keystore name')
  parser.add_option('--stamp', help='Path to touch on success.')

  options, _ = parser.parse_args()

  with tempfile.NamedTemporaryFile() as intermediate_file:
    signed_apk_path = intermediate_file.name
    SignApk(options.key_path, options.key_name, options.key_passwd,
            options.unsigned_apk_path, signed_apk_path)
    AlignApk(options.android_sdk_root, signed_apk_path, options.final_apk_path)

  if options.stamp:
    build_utils.Touch(options.stamp)


if __name__ == '__main__':
  sys.exit(main())


