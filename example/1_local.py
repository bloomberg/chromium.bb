#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, locally in a temporary
directory.

Uses a local hash table instead of a remote isolate server.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

# Pylint can't find common.py that's in the same directory as this file.
# pylint: disable=F0401
import common


def main():
  options = common.parse_args(use_isolate_server=False, use_swarming=False)
  tempdir = tempfile.mkdtemp(prefix='hello_world')
  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    # - cache/
    # - hashtable/
    cachedir = os.path.join(tempdir, 'cache')
    hashtabledir = os.path.join(tempdir, 'hashtable')
    isolateddir = os.path.join(tempdir, 'isolated')
    isolated = os.path.join(isolateddir, 'hello_world.isolated')

    os.mkdir(isolateddir)

    common.note('Archiving to %s' % hashtabledir)
    # TODO(maruel): Parse the output from run() to get 'isolated_sha1'.
    common.run(
        [
          'isolate.py',
          'hashtable',
          '--isolate', os.path.join('payload', 'hello_world.isolate'),
          '--isolated', isolated,
          '--outdir', hashtabledir,
          '--config-variable', 'OS', 'Yours',
        ], options.verbose)

    common.note(
        'Running the executable in a temporary directory from the hash table')
    with open(isolated, 'rb') as f:
      isolated_sha1 = hashlib.sha1(f.read()).hexdigest()
    common.run(
        [
          'run_isolated.py',
          '--cache', cachedir,
          '--indir', hashtabledir,
          '--hash', isolated_sha1,
          # TODO(maruel): Should not require this.
          '--namespace', 'default',
          '--no-log',
        ], options.verbose)
    return 0
  except subprocess.CalledProcessError as e:
    return e.returncode
  finally:
    shutil.rmtree(tempdir)


if __name__ == '__main__':
  sys.exit(main())
