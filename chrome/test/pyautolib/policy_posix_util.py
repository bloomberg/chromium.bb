#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper script to copy policy files into the correct location the machine."""

import os
import shutil
import subprocess
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
  elif sys.argv[1] == 'mcximport':
    assert sys.platform == 'darwin'
    user = sys.argv[2]
    bundle = sys.argv[3]
    mcx_file = sys.argv[4]
    # Note: os.system() drops the euid priviledges on mac.
    # Clear the current preferences.
    subprocess.call(['dscl', '.', '-mcxdelete', '/Users/' + user, bundle, '='])
    # Import the new preferences, if a file was specified.
    if mcx_file:
      subprocess.call(['dscl', '.', '-mcximport', '/Users/' + user, mcx_file])
    # Make sure the cache is refreshed.
    subprocess.call(['mcxrefresh', '-n', user])
  else:
    mac_opt_cmds = ', [mcximport]' if sys.platform == 'darwin' else ''
    print >>sys.stderr, (
        'Invalid syntax. Possible values are [copy], [setup_dir], '
        '[perm_dir], [remove_dir]%s' % mac_opt_cmds)
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(main())
