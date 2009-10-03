#!/usr/bin/python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script helps workaround XCode dependencies bug.

import os
import sys

_SRC_PATH = os.path.join(os.path.dirname(__file__), '..', '..')

def RemoveDir(path):
  for root, dirs, files in os.walk(path, topdown=False):
    for name in files:
        os.remove(os.path.join(root, name))
    for name in dirs:
        os.rmdir(os.path.join(root, name))
  os.rmdir(path)

def main(argv):
  # We need to apply the workaround only on Mac.
  if not sys.platform.lower() in ("darwin", "mac"):
    return 0

  root = os.path.join(_SRC_PATH, 'xcodebuild', 'DerivedSources')
  tail = os.path.join('gen', 'native_client', 'src', 'trusted')

  release_dir = os.path.join(root, 'Release', tail)
  if (os.path.isdir(release_dir)):
    RemoveDir(release_dir)

  debug_dir = os.path.join(root, 'Debug', tail)
  if (os.path.isdir(debug_dir)):
    RemoveDir(debug_dir)

  binaries_dir = os.path.join(root, '..', 'validator_x86.build')
  if (os.path.isdir(binaries_dir)):
    RemoveDir(binaries_dir)

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))