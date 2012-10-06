#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, locally in a temporary
directory.
"""

import getpass
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import time

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def run(cmd):
  print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, '..', cmd[0])] + cmd[1:]
  subprocess.check_call(cmd)


def main():
  # Uncomment to make isolate.py to output logs.
  #os.environ['ISOLATE_DEBUG'] = '1'
  swarm_server = 'http://chromium-swarm.appspot.com'
  cad_server = 'http://isolateserver.appspot.com/'
  prefix = getpass.getuser() + '-'
  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    tempdir = tempfile.mkdtemp(prefix='hello_world')
    isolated = os.path.join(tempdir, 'hello_world.isolated')

    print('Archiving')
    run(
        [
          'isolate.py',
          'hashtable',
          '--isolate', os.path.join(ROOT_DIR, 'hello_world.isolate'),
          '--result', isolated,
          '--outdir', cad_server,
        ])

    print('\nRunning')
    hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
    run(
        [
          'swarm_trigger_step.py',
          '--os_image', sys.platform,
          '--swarm-url', swarm_server,
          '--test-name-prefix', prefix,
          '--data-server', cad_server,
          '--run_from_hash',
          hashval,
          'hello_world',
          '1',
          '',
        ])

    print('\nSleeping a bit; todo: remove me')
    time.sleep(1)

    print('\nGetting results')
    run(
        [
          'swarm_get_results.py',
          '--url', swarm_server,
          prefix + 'hello_world',
        ])
  finally:
    shutil.rmtree(tempdir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
