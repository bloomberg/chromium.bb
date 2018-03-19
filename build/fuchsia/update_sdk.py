#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates the Fuchsia SDK to the given revision. Should be used in a 'hooks_os'
entry so that it only runs when .gclient's target_os includes 'fuchsia'."""

import os
import shutil
import subprocess
import sys
import tarfile
import tempfile

SDK_HASH = '6e46feb3b26db267c65ea0923426a16f4da835bb'

REPOSITORY_ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(REPOSITORY_ROOT, 'build'))

import find_depot_tools


def EnsureDirExists(path):
  if not os.path.exists(path):
    print 'Creating directory %s' % path
    os.makedirs(path)


def main():
  if len(sys.argv) != 1:
    print >>sys.stderr, 'usage: %s' % sys.argv[0]
    return 1

  output_dir = os.path.join(REPOSITORY_ROOT, 'third_party', 'fuchsia-sdk')

  hash_filename = os.path.join(output_dir, '.hash')
  if os.path.exists(hash_filename):
    with open(hash_filename, 'r') as f:
      if f.read().strip() == SDK_HASH:
        # Nothing to do.
        return 0

  print 'Downloading SDK %s...' % SDK_HASH

  if os.path.isdir(output_dir):
    shutil.rmtree(output_dir)

  fd, tmp = tempfile.mkstemp()
  os.close(fd)

  try:
    bucket = 'gs://fuchsia/sdk/linux-amd64/'
    cmd = [os.path.join(find_depot_tools.DEPOT_TOOLS_PATH, 'gsutil.py'),
           'cp', bucket + SDK_HASH, tmp]
    subprocess.check_call(cmd)
    with open(tmp, 'rb') as f:
      EnsureDirExists(output_dir)
      tarfile.open(mode='r:gz', fileobj=f).extractall(path=output_dir)
  finally:
    os.remove(tmp)

  with open(hash_filename, 'w') as f:
    f.write(SDK_HASH)

  return 0


if __name__ == '__main__':
  sys.exit(main())
