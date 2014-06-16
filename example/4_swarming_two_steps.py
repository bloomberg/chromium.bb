#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, remotely on a Swarming
slave.

It compiles and archives via 'isolate.py archive', then discard the local files.
After, it triggers and finally collects the results.
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
  options = common.parse_args(use_isolate_server=True, use_swarming=True)
  try:
    tempdir = tempfile.mkdtemp(prefix='hello_world')
    try:
      # All the files are put in a temporary directory. This is optional and
      # simply done so the current directory doesn't have the following files
      # created:
      # - hello_world.isolated
      # - hello_world.isolated.state
      isolated = os.path.join(tempdir, 'hello_world.isolated')
      common.note('Archiving to %s' % options.isolate_server)
      common.run(
          [
            'isolate.py',
            'archive',
            '--isolate', os.path.join('payload', 'hello_world.isolate'),
            '--isolated', isolated,
            '--isolate-server', options.isolate_server,
            '--config-variable', 'OS', options.swarming_os,
          ], options.verbose)
      with open(isolated, 'rb') as f:
        hashval = hashlib.sha1(f.read()).hexdigest()
    finally:
      shutil.rmtree(tempdir)

    # At this point, the temporary directory is not needed anymore.
    tempdir = None

    common.note('Running on %s' % options.swarming)
    cmd = [
      'swarming.py',
      'trigger',
      '--swarming', options.swarming,
      '--isolate-server', options.isolate_server,
      '--dimension', 'os', options.swarming_os,
      '--task-name', options.task_name,
      hashval,
    ]
    if options.priority is not None:
      cmd.extend(('--priority', str(options.priority)))
    common.run(cmd, options.verbose)

    common.note('Getting results from %s' % options.swarming)
    common.run(
        [
          'swarming.py',
          'collect',
          '--swarming', options.swarming,
          options.task_name,
        ], options.verbose)
    return 0
  except subprocess.CalledProcessError as e:
    print e.returncode or 1


if __name__ == '__main__':
  sys.exit(main())
