#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Runs hello_world.py, through hello_world.isolate, remotely on a Swarming
slave.

It first 'compiles' hello_world.isolate into hello_word.isolated, then requests
via swarming.py to archives, run and collect results for this task.
"""

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
  tempdir = tempfile.mkdtemp(prefix='hello_world')
  try:
    # All the files are put in a temporary directory. This is optional and
    # simply done so the current directory doesn't have the following files
    # created:
    # - hello_world.isolated
    # - hello_world.isolated.state
    isolated = os.path.join(tempdir, 'hello_world.isolated')

    common.note(
        'Creating hello_world.isolated. Note that this doesn\'t archives '
        'anything.')
    common.run(
        [
          'isolate.py',
          'check',
          '--isolate', os.path.join('payload', 'hello_world.isolate'),
          '--isolated', isolated,
          '--config-variable', 'OS', options.swarming_os,
        ], options.verbose)

    common.note(
        'Running the job remotely. This:\n'
        ' - archives to %s\n'
        ' - runs and collect results via %s' %
        (options.isolate_server, options.swarming))
    cmd = [
      'swarming.py',
      'run',
      '--swarming', options.swarming,
      '--isolate-server', options.isolate_server,
      '--dimension', 'os', options.swarming_os,
      '--task-name', options.task_name,
      isolated,
    ]
    if options.idempotent:
      cmd.append('--idempotent')
    if options.priority is not None:
      cmd.extend(('--priority', str(options.priority)))
    common.run(cmd, options.verbose)
    return 0
  except subprocess.CalledProcessError as e:
    return e.returncode
  finally:
    shutil.rmtree(tempdir)


if __name__ == '__main__':
  sys.exit(main())
