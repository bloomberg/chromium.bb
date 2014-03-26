#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

import subprocess2

from git_common import current_branch, get_or_create_merge_base, config_list
from git_common import GIT_EXE

def main(args):
  default_args = config_list('depot-tools.upstream-diff.default-args')
  args = default_args + args

  parser = argparse.ArgumentParser()
  parser.add_argument('--wordwise', action='store_true', default=False,
                      help=(
                        'Print a colorized wordwise diff '
                        'instead of line-wise diff'))
  opts, extra_args = parser.parse_known_args(args)

  cmd = [GIT_EXE, 'diff', '--patience', '-C', '-C']
  if opts.wordwise:
    cmd += ['--word-diff=color', r'--word-diff-regex=(\w+|[^[:space:]])']
  cmd += [get_or_create_merge_base(current_branch())]

  cmd += extra_args

  subprocess2.check_call(cmd)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
