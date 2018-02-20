#!/usr/bin/env python
#
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signs and aligns an APK."""

import argparse
import shutil
import subprocess
import tempfile


def main():
  parser = argparse.ArgumentParser()

  parser.add_argument('--apksigner-path', required=True,
                      help='Path to the apksigner executable.')
  parser.add_argument('--zipalign-path', required=True,
                      help='Path to the zipalign executable.')
  parser.add_argument('--unsigned-apk-path', required=True,
                      help='Path to input unsigned APK.')
  parser.add_argument('--final-apk-path', required=True,
                      help='Path to output signed and aligned APK.')
  parser.add_argument('--key-path', required=True,
                      help='Path to keystore for signing.')
  parser.add_argument('--key-passwd', required=True,
                      help='Keystore password')
  parser.add_argument('--key-name', required=True,
                      help='Keystore name')
  options = parser.parse_args()

  # Use a tempfile so that Ctrl-C does not leave the file with a fresh mtime
  # and a corrupted state.
  with tempfile.NamedTemporaryFile() as staging_file:
    # v2 signing requires that zipalign happen first.
    subprocess.check_output([
        options.zipalign_path, '-p', '-f', '4',
        options.unsigned_apk_path, staging_file.name])
    subprocess.check_output([
        options.apksigner_path, 'sign',
        '--in', staging_file.name,
        '--out', staging_file.name,
        '--ks', options.key_path,
        '--ks-key-alias', options.key_name,
        '--ks-pass', 'pass:' + options.key_passwd,
    ])
    shutil.move(staging_file.name, options.final_apk_path)
    staging_file.delete = False


if __name__ == '__main__':
  main()
