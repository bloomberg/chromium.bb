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
  args = parser.parse_args()

  dm_path = os.path.join(SKIA_SRC_DIR, 'out', 'Debug', 'dm')
  skps_dir = os.path.join(REPOS_BASE_DIR, 'skps', 'slave%d' % args.slave_num)
  resource_path = os.path.join(SKIA_SRC_DIR, 'resources')

  # TODO(rmistry): Double check the below DM configuration with mtklein@. We
  # need a basic configuration that can help catch bugs like the ones listed in
  # skbug.com/4416
  cmd = [
      dm_path,
      '--src', 'skp',
      '--skps', skps_dir,
      '--resourcePath', resource_path,
      '--config', '8888',
      '--verbose',
  ]
  return subprocess.call(cmd)


if __name__ == '__main__':
  sys.exit(main())
