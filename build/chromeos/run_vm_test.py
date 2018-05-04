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
# cros_vm is a tool for managing VMs.
CROS_VM_PATH = os.path.abspath(os.path.join(
    CHROMITE_PATH, 'bin', 'cros_vm'))
# cros_run_vm_test is a helper tool for running tests inside VMs and wraps
# cros_vm.
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


def host_cmd(args):
  if not args.cmd:
    logging.error('Must specify command to run on the host.')
    return 1

  cros_vm_cmd_args = [
      CROS_VM_PATH,
      '--board', args.board,
      '--cache-dir', args.cros_cache,
  ]
  if args.verbose:
    cros_vm_cmd_args.append('--debug')

  rc = subprocess.call(
      cros_vm_cmd_args + ['--start'], stdout=sys.stdout, stderr=sys.stderr)
  if rc:
    logging.error('VM start-up failed. Quitting early.')
    return rc

  try:
    return subprocess.call(args.cmd, stdout=sys.stdout, stderr=sys.stderr)
  finally:
    rc = subprocess.call(
        cros_vm_cmd_args + ['--stop'], stdout=sys.stdout, stderr=sys.stderr)
    if rc:
      logging.error('VM tear-down failed.')


def vm_test(args):
  cros_run_vm_test_cmd = [
      CROS_RUN_VM_TEST_PATH,
      '--start',
      '--board', args.board,
      '--cache-dir', args.cros_cache,
      '--cwd', os.path.relpath(args.path_to_outdir, CHROMIUM_SRC_PATH),
  ]

  # cros_run_vm_test has trouble with relative paths that go up directories, so
  # cd to src/, which should be the root of all data deps.
  os.chdir(CHROMIUM_SRC_PATH)

  for f in read_runtime_files(args.runtime_deps_path, args.path_to_outdir):
    cros_run_vm_test_cmd.extend(['--files', f])

  if args.test_launcher_summary_output:
    result_dir, result_file = os.path.split(args.test_launcher_summary_output)
    # If args.test_launcher_summary_output is a file in cwd, result_dir will be
    # an empty string, so replace it with '.' when this is the case so
    # cros_run_vm_test can correctly handle it.
    if not result_dir:
      result_dir = '.'
    vm_result_file = '/tmp/%s' % result_file
    cros_run_vm_test_cmd += [
      '--results-src', vm_result_file,
      '--results-dest-dir', result_dir,
    ]

  cros_run_vm_test_cmd += [
      '--cmd',
      '--',
      './' + args.test_exe,
      '--test-launcher-shard-index=%d' % args.test_launcher_shard_index,
      '--test-launcher-total-shards=%d' % args.test_launcher_total_shards,
  ]

  if args.test_launcher_summary_output:
    cros_run_vm_test_cmd += [
      '--test-launcher-summary-output=%s' % vm_result_file,
    ]

  logging.info('Running the following command:')
  logging.info(' '.join(cros_run_vm_test_cmd))

  return subprocess.call(
      cros_run_vm_test_cmd, stdout=sys.stdout, stderr=sys.stderr)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--verbose', '-v', action='store_true')
  # Required args.
  parser.add_argument(
      '--board', type=str, required=True, help='Type of CrOS device.')
  subparsers = parser.add_subparsers(dest='test_type')
  # Host-side test args.
  host_cmd_parser = subparsers.add_parser(
      'host-cmd',
      help='Runs a host-side test. Pass the host-side command to run after '
           '"--". Hostname and port for the VM will be 127.0.0.1:9222.')
  host_cmd_parser.set_defaults(func=host_cmd)
  host_cmd_parser.add_argument(
      '--cros-cache', type=str, required=True, help='Path to cros cache.')
  host_cmd_parser.add_argument('cmd', nargs=argparse.REMAINDER)
  # VM-side test args.
  vm_test_parser = subparsers.add_parser(
      'vm-test',
      help='Runs a vm-side gtest.')
  vm_test_parser.set_defaults(func=vm_test)
  vm_test_parser.add_argument(
      '--cros-cache', type=str, required=True, help='Path to cros cache.')
  vm_test_parser.add_argument(
      '--test-exe', type=str, required=True,
      help='Path to test executable to run inside VM.')
  vm_test_parser.add_argument(
      '--path-to-outdir', type=str, required=True,
      help='Path to output directory, all of whose contents will be deployed '
           'to the device.')
  vm_test_parser.add_argument(
      '--runtime-deps-path', type=str,
      help='Runtime data dependency file from GN.')
  vm_test_parser.add_argument(
      '--test-launcher-summary-output', type=str,
      help='When set, will pass the same option down to the test and retrieve '
           'its result file at the specified location.')
  vm_test_parser.add_argument(
      '--test-launcher-shard-index',
      type=int, default=os.environ.get('GTEST_SHARD_INDEX', 0),
      help='Index of the external shard to run.')
  vm_test_parser.add_argument(
      '--test-launcher-total-shards',
      type=int, default=os.environ.get('GTEST_TOTAL_SHARDS', 1),
      help='Total number of external shards.')
  args = parser.parse_args()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.WARN)

  if not os.path.exists('/dev/kvm'):
    logging.error('/dev/kvm is missing. Is KVM installed on this machine?')
    return 1
  elif not os.access('/dev/kvm', os.W_OK):
    logging.error(
        '/dev/kvm is not writable as current user. Perhaps you should be root?')
    return 1

  args.cros_cache = os.path.abspath(args.cros_cache)
  return args.func(args)


if __name__ == '__main__':
  sys.exit(main())
