#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper script to install Xcode. For use on the bots.

Installing Xcode requires sudo. This script will be whitelisted in /etc/sudoers
so that we can easily update Xcode on our bots.
"""

import argparse
import hashlib
import logging
import os
import subprocess
import sys


# List of sha256 hashes of Xcode package file
# Contents/Resources/Packages/XcodeSystemResources.pkg
WHITELISTED_PACKAGES = [
  # Xcode versions 8A218a-1, 8A218a-2
  '3d948c4bd7c8941478a60aece3cb98ef936a57a4cc1c8f8d5f7eef70d0ebbad1',
  # Xcode versions 8B62-1, 8C1002-1
  '21ed3271af2ac7d67c6c09a9481a1fd5bb886950990365bb936dde7585e09061',
  # Xcode versions 8E2002-1
  'ff2377d3976f7acf2adfe5ca91c2e6cc59dd112647efa0bf39e9d9decd2ee1b3',
  # Xcode versions 9M136h-1
  '2ec798b123bcfa7f8c0e5618c193ecd26ddce87f2e27f6b5d2fb0720926e5464',
  # Xcode versions 9M137d-1
  '6c5f4a2fd6dc477f8f06ccd6f6119da540dd5fe3a6036357dbe9c13d611fc366',
]

def main():
  logging.basicConfig(level=logging.DEBUG)

  parser = argparse.ArgumentParser(
      description='Wrapper script to install Xcode.')
  parser.add_argument(
      '--package-path',
      help='Path to Xcode package to install.',
      required=True)
  args = parser.parse_args()

  if not os.path.isfile(args.package_path):
    logging.critical('Path %s is not a file.' % args.package_path)
    return 1

  sha256 = hashlib.sha256()
  with open(args.package_path, 'rb') as f:
    for chunk in iter(lambda: f.read(8192), b''):
      sha256.update(chunk)
  sha256_hash = sha256.hexdigest()

  if sha256_hash not in WHITELISTED_PACKAGES:
    logging.critical('Attempted to install a non-whitelisted Xcode package.')
    return 1

  pipe = subprocess.Popen(
      ['/usr/sbin/installer', '-pkg', args.package_path, '-target', '/'],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  stdout, stderr = pipe.communicate()

  for line in stdout.splitlines():
    logging.debug(line)
  for line in stderr.splitlines():
    logging.error(line)

  return pipe.returncode


if __name__ == '__main__':
  sys.exit(main())
