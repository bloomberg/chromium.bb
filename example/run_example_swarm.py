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

# Mapping of the sys.platform value into Swarm OS value.
OSES = {'win32': 'win', 'linux2': 'linux', 'darwin': 'mac'}


def run(cmd, verbose):
  cmd = cmd[:]
  cmd.extend(['--verbose'] * verbose)
  print('Running: %s' % ' '.join(cmd))
  cmd = [sys.executable, os.path.join(ROOT_DIR, '..', cmd[0])] + cmd[1:]
  if sys.platform != 'win32':
    cmd = ['time', '-p'] + cmd
  subprocess.check_call(cmd)


def simple(isolate_server, swarming_server, prefix, os_slave, verbose):
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
          # Manually override the OS.
          '--variable', 'OS', OSES[os_slave],
        ],
        verbose)

    run(
        [
          'swarming.py',
          'run',
          '--os', os_slave,
          '--swarming', swarming_server,
          '--task-prefix', prefix,
          '--isolate-server', isolate_server,
          isolated,
        ],
        verbose)
    return 0
  finally:
    shutil.rmtree(tempdir)


def involved(isolate_server, swarming_server, prefix, os_slave, verbose):
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
          'archive',
          '--isolate', os.path.join(ROOT_DIR, 'hello_world.isolate'),
          '--isolated', isolated,
          '--outdir', isolate_server,
          # Manually override the OS.
          '--variable', 'OS', OSES[os_slave],
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
        '--os', os_slave,
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
      metavar='URL', default='',
      help='Isolate server to use')
  parser.add_option(
      '-S', '--swarming',
      metavar='URL', default='',
      help='Swarming server to use')
  parser.add_option('-v', '--verbose', action='count', default=0)
  parser.add_option(
      '-o', '--os', default=sys.platform,
      help='Swarm OS image to request. Should be one of the valid sys.platform '
           'values like darwin, linux2 or win32 default: %default.')
  parser.add_option(
      '--short', action='store_true',
      help='Use \'swarming.py run\' instead of running each step manually')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)
  if not options.isolate_server:
    parser.error('--isolate-server is required.')
  if not options.swarming:
    parser.error('--swarming is required.')

  prefix = getpass.getuser() + '-' + datetime.datetime.now().isoformat() + '-'
  try:
    if options.short:
      return simple(
          options.isolate_server,
          options.swarming,
          prefix,
          options.os,
          options.verbose)
    else:
      return involved(
          options.isolate_server,
          options.swarming,
          prefix,
          options.os,
          options.verbose)
  except subprocess.CalledProcessError as e:
    print e.returncode or 1


if __name__ == '__main__':
  sys.exit(main())
