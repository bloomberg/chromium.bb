#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"This script copies a directory into another directory, replacing all files."

import argparse
import logging
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.file_tools
import pynacl.log_tools

def main(args):
  parser = argparse.ArgumentParser()

  # List out global options for all commands.
  parser.add_argument(
    '-v', '--verbose', dest='verbose',
    action='store_true', default=False,
    help='Verbose output')
  parser.add_argument(
    '-q', '--quiet', dest='quiet',
    action='store_true', default=False,
    help='Quiet output')
  parser.add_argument(
    '--stamp-arg', dest='stamp_arg',
    help='Optional gyp stamp argument for better error messages.')
  parser.add_argument(
    '--stamp-file', dest='stamp_file', required=True,
    help='Stamp file within the source directory to check up to date status.')
  parser.add_argument(
    'source', help='Source directory to copy from.')
  parser.add_argument(
    'dest', help='Destination directory to copy into.')

  arguments = parser.parse_args(args)
  pynacl.log_tools.SetupLogging(verbose=arguments.verbose,
                                quiet=arguments.quiet)

  source_dir = os.path.abspath(os.path.normpath(arguments.source))
  dest_dir = os.path.abspath(os.path.normpath(arguments.dest))

  stamp_file = os.path.join(source_dir, arguments.stamp_file)
  abs_stamp_file = os.path.abspath(os.path.normpath(stamp_file))

  if not os.path.isdir(source_dir):
    logging.error('Invalid source directory: %s', arguments.source)
    return 1
  elif not arguments.stamp_file:
    logging.error('No stamp file specified.')
    if arguments.stamp_arg:
      logging.warn('Set stamp file using gyp argument: %s',
                   arguments.stamp_arg)
    return 1
  elif not os.path.isfile(abs_stamp_file):
    logging.error('Invalid stamp file: %s', arguments.stamp_file)
    if arguments.stamp_arg:
      logging.warn('Did you forget to setup gyp argument (%s)?',
                   arguments.stamp_arg)
    return 1
  elif not abs_stamp_file.startswith(source_dir + os.sep):
    logging.error('Stamp file (%s) must be within the source directory: %s',
                  arguments.stamp_file, arguments.source)
    return 1

  logging.info('Copying "%s" -> "%s"', source_dir, dest_dir)
  pynacl.file_tools.RemoveDirectoryIfPresent(dest_dir)
  pynacl.file_tools.CopyTree(source_dir, dest_dir)
  return 0

if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except Exception as e:
    print >> sys.stderr, e
    sys.exit(1)
