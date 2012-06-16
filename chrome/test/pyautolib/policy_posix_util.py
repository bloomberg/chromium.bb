#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to copy policy files into the correct location the machine."""

import os
import shutil
import sys


def main():
  assert os.geteuid() == 0, 'Need superuser privileges'
  if sys.argv[1] == 'copy':
    assert os.path.isdir(sys.argv[3])
    shutil.copy(sys.argv[2], sys.argv[3])
    dirList = os.listdir(sys.argv[3])
    for fname in dirList:
      filename = os.path.join(sys.argv[3], fname)
      os.chmod(filename, 0755)
  elif sys.argv[1] == 'setup_dir':
    os.makedirs(sys.argv[2])
  elif sys.argv[1] == 'perm_dir':
    os.system('chmod -R 755 "%s/../.."' % sys.argv[2])
  elif sys.argv[1] == 'remove_dir':
    os.system('rm -rf "%s"' % sys.argv[2])
  else:
    print >>sys.stderr, (
        'Invalid syntax. Possible values are [copy], [setup_dir], '
        '[perm_dir], [remove_dir]')
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
