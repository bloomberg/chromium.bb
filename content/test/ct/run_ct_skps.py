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

REPOS_BASE_DIR = os.path.normpath(os.path.join(
    PARENT_DIR, os.pardir, os.pardir, os.pardir, os.pardir))

SKIA_SRC_DIR = os.path.join(REPOS_BASE_DIR, 'skia')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('-s', '--slave_num', required=True, type=int,
                      help='The slave num of this CT run.')
  parser.add_argument('-t', '--tool', required=True,
                      choices=['dm', 'nanobench', 'get_images_from_skps'],
                      help='The tool to run on the SKPs.')
  parser.add_argument('-g', '--git_hash', required=True,
                      help='The Skia hash the tool was built at.')
  parser.add_argument('-c', '--configuration', required=True,
                      help='The build configuration to use.')
  parser.add_argument('-i', '--isolated_outdir', required=True,
                      help='Swarming will automatically upload to '
                           'isolateserver all artifacts in this dir.')
  args = parser.parse_args()

  tool_path = os.path.join(SKIA_SRC_DIR, 'out', args.configuration, args.tool)
  skps_dir = os.path.join(REPOS_BASE_DIR, 'skps', 'slave%d' % args.slave_num)
  resource_path = os.path.join(SKIA_SRC_DIR, 'resources')

  cmd = [tool_path]
  if args.tool == 'dm':
    # Add DM specific arguments.
    cmd.extend([
      '--src', 'skp',
      '--skps', skps_dir,
      '--resourcePath', resource_path,
      '--config', '8888',
      '--verbose',
    ])
  elif args.tool == 'nanobench':
    # Add Nanobench specific arguments.
    out_results_file = os.path.join(
        args.isolated_outdir, 'nanobench_%s_slave%d.json' % (args.git_hash,
                                                             args.slave_num))
    cmd.extend([
      '--skps', skps_dir,
      '--match', 'skp',
      '--resourcePath', resource_path,
      '--config', '8888', 'gpu',
      '--outResultsFile', out_results_file,
      '--properties', 'gitHash', args.git_hash,
      '--key', 'arch', 'x86_64', 'compiler', 'GCC', 'cpu_or_gpu', 'CPU',
               'cpu_or_gpu_value', 'AVX2', 'model', 'SWARM', 'os', 'Ubuntu',
      '--verbose',
    ])
  elif args.tool == 'get_images_from_skps':
    # Add get_images_from_skps specific arguments.
    img_out = os.path.join(args.isolated_outdir, 'img_out')
    os.makedirs(img_out)
    failures_json_out = os.path.join(args.isolated_outdir, 'failures.json')
    cmd.extend([
      '--out', img_out,
      '--skps', skps_dir,
      '--failuresJsonPath', failures_json_out,
      '--writeImages', 'false',
      '--testDecode',
    ])

  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
