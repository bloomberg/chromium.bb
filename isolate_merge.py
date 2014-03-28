#!/usr/bin/env python
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

"""Merges multiple OS-specific gyp dependency lists into one that works on all
of them.

The logic is relatively simple. Takes the current conditions, add more
condition, find the strict subset. Done.
"""

import logging
import os
import sys

import isolate_format

from utils import tools


def load_isolates(items):
  """Parses each .isolate file and returns the merged results.

  It only loads what load_isolate_as_config() can process.

  Return values:
    files: dict(filename, set(OS where this filename is a dependency))
    dirs:  dict(dirame, set(OS where this dirname is a dependency))
    oses:  set(all the OSes referenced)
  """
  # pylint: disable=W0212
  configs = isolate_format.Configs(None, ())
  for item in items:
    item = os.path.abspath(item)
    logging.debug('loading %s' % item)
    if item == '-':
      content = sys.stdin.read()
    else:
      with open(item, 'r') as f:
        content = f.read()
    new_config = isolate_format.load_isolate_as_config(
        os.path.dirname(item),
        isolate_format.eval_content(content),
        isolate_format.extract_comment(content))
    logging.debug('has configs: ' + ','.join(map(repr, new_config._by_config)))
    configs = configs.union(new_config)
  logging.debug('Total configs: ' + ','.join(map(repr, configs._by_config)))
  return configs


def main(args=None):
  tools.disable_buffering()
  parser = tools.OptionParserWithLogging(
      usage='%prog <options> [file1] [file2] ...')
  parser.add_option(
      '-o', '--output', help='Output to file instead of stdout')

  options, args = parser.parse_args(args)

  configs = load_isolates(args)
  data = configs.make_isolate_file()
  if options.output:
    with open(options.output, 'wb') as f:
      isolate_format.print_all(configs.file_comment, data, f)
  else:
    isolate_format.print_all(configs.file_comment, data, sys.stdout)
  return 0


if __name__ == '__main__':
  sys.exit(main())
