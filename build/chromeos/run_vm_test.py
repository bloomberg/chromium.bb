#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import logging
import os
import re
import stat
import subprocess
import sys


CHROMIUM_SRC_PATH = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))
CHROMITE_PATH = os.path.abspath(os.path.join(
    CHROMIUM_SRC_PATH, 'third_party', 'chromite'))
CROS_RUN_VM_TEST_PATH = os.path.abspath(os.path.join(
    CHROMITE_PATH, 'bin', 'cros_run_vm_test'))


_FILE_BLACKLIST = [
  re.compile(r'.*build/chromeos.*'),
  re.compile(r'.*build/cros_cache.*'),
  re.compile(r'.*third_party/chromite.*'),
]


def read_runtime_files(runtime_deps_path, outdir):
  if not runtime_deps_path:
    return []

  abs_runtime_deps_path = os.path.abspath(
      os.path.join(outdir, runtime_deps_path))
  with open(abs_runtime_deps_path) as runtime_deps_file:
    files = [l.strip() for l in runtime_deps_file if l]
  rel_file_paths = []
  for f in files:
    rel_file_path = os.path.relpath(
        os.path.abspath(os.path.join(outdir, f)),
        os.getcwd())
    if not any(regex.match(rel_file_path) for regex in _FILE_BLACKLIST):
      rel_file_paths.append(rel_file_path)

  return rel_file_paths


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose', '-v', action='store_true')
  parser.add_argument(
      '--path-to-outdir', type=str, required=True,
      help='Path to output directory, all of whose contents will be deployed '
           'to the device.')
  parser.add_argument(
      '--board', type=str, required=True, help='Type of CrOS device.')
  parser.add_argument(
      '--test-exe', type=str, required=True,
      help='Path to test executable to run inside VM.')
  parser.add_argument(
      '--runtime-deps-path', type=str,
      help='Runtime data dependency file from GN.')
  parser.add_argument(
      '--cros-cache', type=str, required=True, help='Path to cros cache.')
  args, unknown_args = parser.parse_known_args()

  if unknown_args:
    print >> sys.stderr, 'Ignoring unknown args: %s' % unknown_args

  args.cros_cache = os.path.abspath(os.path.join(
      args.path_to_outdir, args.cros_cache))
  cros_run_vm_test_cmd = [
      CROS_RUN_VM_TEST_PATH,
      '--start',
      '--board', args.board,
      '--cache-dir', args.cros_cache,
      '--cwd', os.path.relpath(args.path_to_outdir, CHROMIUM_SRC_PATH),
  ]
  if args.verbose:
    cros_run_vm_test_cmd.append('--debug')

  # cros_run_vm_test has trouble with relative paths that go up directors, so
  # cd to src/, which should be the root of all data deps.
  os.chdir(CHROMIUM_SRC_PATH)

  for f in read_runtime_files(args.runtime_deps_path, args.path_to_outdir):
    cros_run_vm_test_cmd.extend(['--files', f])

  cros_run_vm_test_cmd += [
      '--cmd',
      '--',
      './' + args.test_exe,
  ]

  print 'Running the following command:'
  print ' '.join(cros_run_vm_test_cmd)
  os.execv(CROS_RUN_VM_TEST_PATH, cros_run_vm_test_cmd)


if __name__ == '__main__':
  sys.exit(main())
