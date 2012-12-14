#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolated, locally in a temporary
directory with the files fetched from the remote Content-Addressed Datastore.
"""

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
  parser.add_option('-v', '--verbose', action='store_true')
  options, args = parser.parse_args()
  if args:
    parser.error('Unsupported argument %s' % args)
  if options.verbose:
    os.environ['ISOLATE_DEBUG'] = '1'

  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    # - cache/
    tempdir = tempfile.mkdtemp(prefix='hello_world')
    cachedir = os.path.join(tempdir, 'cache')
    isolateddir = os.path.join(tempdir, 'isolated')
    isolated = os.path.join(isolateddir, 'hello_world.isolated')

    os.mkdir(isolateddir)

    print('Archiving')
    run(
        [
          'isolate.py',
          'hashtable',
          '--isolate', os.path.join(ROOT_DIR, 'hello_world.isolate'),
          '--isolated', isolated,
          '--outdir', options.isolate_server,
        ])

    print('\nRunning')
    # TODO(maruel): This makes it not work with NFS.
    remote = (
        options.isolate_server.rstrip('/') + '/content/retrieve/default-gzip/')
    hashval = hashlib.sha1(open(isolated, 'rb').read()).hexdigest()
    run(
        [
          'run_isolated.py',
          '--cache', cachedir,
          '--remote', remote,
          '--hash', hashval,
        ])
  finally:
    shutil.rmtree(tempdir)
  return 0


if __name__ == '__main__':
  sys.exit(main())
