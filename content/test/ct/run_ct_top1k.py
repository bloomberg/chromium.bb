#!/usr/bin/env python
# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is meant to be run on a Swarming bot."""

import argparse
import os
import subprocess
import sys


PARENT_DIR = os.path.dirname(os.path.realpath(__file__))


def _GetChromiumSrcDir():
  return os.path.abspath(os.path.join(
      PARENT_DIR, os.pardir, os.pardir, os.pardir))


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-s', '--slave_num', required=True,
                      help='The slave num of this CT run.')
  parser.add_argument('-b', '--benchmark', required=True,
                      help='The benchmark to run.')
  parser.add_argument('-m', '--master', required=True,
                      help='The master the builder is running on.')
  parser.add_argument('-c', '--builder', required=True,
                      help='The builder that triggered this run.')
  parser.add_argument('-g', '--git_hash', required=True,
                      help='The git hash the build was triggered at.')

  args = parser.parse_args()

  ct_binary_path = os.path.join(PARENT_DIR, 'run_chromium_perf_swarming')
  chromium_binary_path = os.path.join(_GetChromiumSrcDir(), 'out', 'Release')
  page_sets_dir = os.path.join(
      PARENT_DIR, 'slave%s' % args.slave_num, 'page_sets')
  telemetry_binaries_dir = os.path.join(_GetChromiumSrcDir(), 'tools', 'perf')

  # Run Cluster Telemetry.
  cmd = [
      ct_binary_path,
      '--worker_num', args.slave_num,
      '--chromium_build', chromium_binary_path,
      '--benchmark_name', args.benchmark,
      '--telemetry_binaries_dir', telemetry_binaries_dir,
      '--page_sets_dir', page_sets_dir,
      '--master', args.master,
      '--builder', args.builder,
      '--git_hash', args.git_hash,
      '--alsologtostderr',
  ]
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
