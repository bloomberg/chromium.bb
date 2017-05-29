#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a script to run a Fushsia test (typically on QEMU) by delegating to
build/fuchsia/test_runner.py.
"""

import argparse
import os
import pipes
import re
import sys


SCRIPT_TEMPLATE = """\
#!/bin/sh
exec {test_runner_path} {test_runner_args} "$@"
"""


def MakeDirectory(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


def WriteDepfile(depfile_path, first_gn_output, inputs=None):
  assert depfile_path != first_gn_output
  inputs = inputs or []
  MakeDirectory(os.path.dirname(depfile_path))
  # Ninja does not support multiple outputs in depfiles.
  with open(depfile_path, 'w') as depfile:
    depfile.write(first_gn_output.replace(' ', '\\ '))
    depfile.write(': ')
    depfile.write(' '.join(i.replace(' ', '\\ ') for i in inputs))
    depfile.write('\n')


def main(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--script-output-path',
                      help='Output path for executable script.')
  parser.add_argument('--depfile',
                      help='Path to the depfile. This must be specified as '
                           'the action\'s first output.')
  parser.add_argument('--test-runner-path',
                      help='Path to test_runner.py (optional).')
  group = parser.add_argument_group('Test runner path arguments.')
  group.add_argument('--output-directory')
  group.add_argument('--runtime-deps-path')
  group.add_argument('--test-name')
  args, test_runner_args = parser.parse_known_args(args)

  def ResolvePath(path):
    return os.path.abspath(os.path.join(
        os.path.dirname(args.script_output_path), '..', path))

  test_runner_path = args.test_runner_path or os.path.join(
      os.path.dirname(__file__), 'test_runner.py')
  test_runner_path = ResolvePath(test_runner_path)

  if args.output_directory:
    test_runner_args.extend(
        ['--output-directory', ResolvePath(args.output_directory)])
  if args.runtime_deps_path:
    test_runner_args.extend(
        ['--runtime-deps-path', ResolvePath(args.runtime_deps_path)])
  if args.test_name:
    test_runner_args.extend(['--test-name', args.test_name])

  with open(args.script_output_path, 'w') as script:
    script.write(SCRIPT_TEMPLATE.format(
        test_runner_path=test_runner_path,
        test_runner_args=' '.join(pipes.quote(x) for x in test_runner_args)))

  os.chmod(args.script_output_path, 0750)

  if args.depfile:
    WriteDepfile(args.depfile, args.script_output_path,
                 [__file__])


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
