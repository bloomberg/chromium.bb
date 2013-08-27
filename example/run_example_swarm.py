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
sys.path.insert(0, os.path.dirname(ROOT_DIR))

import swarming


def run(cmd, verbose):
  if verbose:
    cmd = cmd + ['--verbose']
  print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, '..', cmd[0])] + cmd[1:]
  if sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def simple(isolate_server, swarming_server, prefix, verbose):
  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    tempdir = tempfile.mkdtemp(prefix='hello_world')
    isolated = os.path.join(tempdir, 'hello_world.isolated')

    run(
        [
          'isolate.py',
          'check',
          '--isolate', os.path.join(ROOT_DIR, 'hello_world.isolate'),
          '--isolated', isolated,
        ],
        verbose)

    run(
        [
          'swarming.py',
          'run',
          '--os', sys.platform,
          '--swarming', swarming_server,
          '--task-prefix', prefix,
          '--isolate-server', isolate_server,
          isolated,
        ],
        verbose)
    return 0
  finally:
    shutil.rmtree(tempdir)


def involved(isolate_server, swarming_server, prefix, verbose):
  """Runs all the steps involved individually, for demonstration purposes."""
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
          '--outdir', isolate_server,
        ],
        verbose)
    hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
  finally:
    shutil.rmtree(tempdir)

  print('\nRunning')
  run(
      [
        'swarming.py',
        'trigger',
        '--os', sys.platform,
        '--isolate-server', isolate_server,
        '--swarming', swarming_server,
        '--task-prefix', prefix,
        '--task',
        hashval,
        'hello_world',
        # Number of shards.
        '1',
        '*',
      ],
      verbose)

  print('\nGetting results')
  run(
      [
        'swarming.py',
        'collect',
        '--swarming', swarming_server,
        prefix + 'hello_world',
      ],
      verbose)
  return 0


def main():
  parser = optparse.OptionParser(description=sys.modules[__name__].__doc__)
  parser.add_option(
      '-I', '--isolate-server',
      default=swarming.ISOLATE_SERVER,
      help='Isolate server to use default: %default')
  parser.add_option(
      '-S', '--swarming',
      default=swarming.SWARM_SERVER,
      help='Isolate server to use default: %default')
  parser.add_option('-v', '--verbose', action='store_true')
  parser.add_option(
      '--short', action='store_true',
      help='Use \'swarming.py run\' instead of running each step manually')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)

  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'
  if options.short:
    return simple(
        options.isolate_server, options.swarming, prefix, options.verbose)
  else:
    return involved(
        options.isolate_server, options.swarming, prefix, options.verbose)


if __name__ == '__main__':
  sys.exit(main())
