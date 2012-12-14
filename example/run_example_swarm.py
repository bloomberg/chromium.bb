#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, remotely on a Swarm slave.
"""

import datetime
import getpass
import hashlib
import optparse
import os
import shutil
import subprocess
import sys
import tempfile

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def run(cmd):
  print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, '..', cmd[0])] + cmd[1:]
  if sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-i', '--isolate-server',
      default='https://isolateserver.appspot.com/',
      help='Isolate server to use default:%default')
  parser.add_option(
      '-s', '--swarm-server',
      default='https://chromium-swarm.appspot.com/',
      help='Isolate server to use default:%default')
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)
  if options.verbose:
    os.environ['ISOLATE_DEBUG'] = '1'

  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'

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
          '--isolated', isolated,
          '--outdir', options.isolate_server,
        ])

    # Note that swarm_trigger_and_get_results.py could be used to run and get
    # results as a single call.
    print('\nRunning')
    hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
    run(
        [
          'swarm_trigger_step.py',
          '--os_image', sys.platform,
          '--swarm-url', options.swarm_server,
          '--test-name-prefix', prefix,
          '--data-server', options.isolate_server,
          '--run_from_hash', hashval,
          'hello_world',
          # Number of shards.
          '1',
          '',
        ])

    print('\nGetting results')
    run(
        [
          'swarm_get_results.py',
          '--url', options.swarm_server,
          prefix + 'hello_world',
        ])
  finally:
    shutil.rmtree(tempdir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
