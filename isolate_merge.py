#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges multiple OS-specific gyp dependency lists into one that works on all
of them.

The logic is relatively simple. Takes the current conditions, add more
condition, find the strict subset. Done.
"""

import logging
import os
import sys

from isolate import eval_content, extract_comment
from isolate import load_isolate_as_config, print_all, union
import run_test_cases


def load_isolates(items):
  """Parses each .isolate file and returns the merged results.

  It only loads what load_isolate_as_config() can process.

  Return values:
    files: dict(filename, set(OS where this filename is a dependency))
    dirs:  dict(dirame, set(OS where this dirname is a dependency))
    oses:  set(all the OSes referenced)
    """
  configs = None
  for item in items:
    item = os.path.abspath(item)
    logging.debug('loading %s' % item)
    if item == '-':
      content = sys.stdin.read()
    else:
      with open(item, 'r') as f:
        content = f.read()
    new_config = load_isolate_as_config(
        os.path.dirname(item),
        eval_content(content),
        extract_comment(content))
    logging.debug('has configs: ' + ','.join(map(repr, new_config.by_config)))
    configs = union(configs, new_config)
  logging.debug('Total configs: ' + ','.join(map(repr, configs.by_config)))
  return configs


def main(args=None):
  run_test_cases.run_isolated.disable_buffering()
  parser = run_test_cases.OptionParserWithLogging(
      usage='%prog <options> [file1] [file2] ...')
  parser.add_option(
      '-o', '--output', help='Output to file instead of stdout')

  options, args = parser.parse_args(args)

  configs = load_isolates(args)
  data = configs.make_isolate_file()
  if options.output:
    with open(options.output, 'wb') as f:
      print_all(configs.file_comment, data, f)
  else:
    print_all(configs.file_comment, data, sys.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
