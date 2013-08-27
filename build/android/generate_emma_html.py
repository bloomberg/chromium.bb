#!/usr/bin/env python

# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Aggregates EMMA coverage files to produce html output."""

import fnmatch
import json
import optparse
import os
import sys
import traceback

from pylib import cmd_helper
from pylib import constants


def _GetFilesWithExt(root_dir, ext):
  """Gets all files with a given extension.

  Args:
    root_dir: Directory in which to search for files.
    ext: Extension to look for (including dot)

  Returns:
    A list of absolute paths to files that match.
  """
  files = []
  for root, _, filenames in os.walk(root_dir):
    basenames = fnmatch.filter(filenames, '*.' + ext)
    files.extend([os.path.join(root, basename)
                  for basename in basenames])

  return files


def main(argv):
  option_parser = optparse.OptionParser()
  option_parser.add_option('--output', help='HTML output filename.')
  option_parser.add_option('--coverage-dir', default=None,
                           help=('Root of the directory in which to search for '
                                 'coverage data (.ec) files.'))
  option_parser.add_option('--metadata-dir', default=None,
                           help=('Root of the directory in which to search for '
                                 'coverage metadata (.em) files.'))
  option_parser.add_option('--cleanup', action='store_true',
                           help='If set, removes coverage/metadata files.')
  options, args = option_parser.parse_args()

  if not (options.coverage_dir and options.metadata_dir and options.output):
    option_parser.error('One or more mandatory options are missing.')

  coverage_files = _GetFilesWithExt(options.coverage_dir, 'ec')
  metadata_files = _GetFilesWithExt(options.metadata_dir, 'em')
  print 'Found coverage files: %s' % str(coverage_files)
  print 'Found metadata files: %s' % str(metadata_files)
  sources_files = []
  final_metadata_files = []
  err = None
  for f in metadata_files:
    sources_file = os.path.splitext(f)[0] + '_sources.txt'
    # TODO(gkanwar): Remove this once old coverage.em files have been cleaned
    # from all bots.
    # Warn if we have old metadata files lying around that don't correspond
    # to a *_sources.txt (these should be manually cleaned).
    try:
      with open(sources_file, 'r') as sf:
        sources_files.extend(json.load(sf))
    except IOError as e:
      traceback.print_exc()
      err = e
    else:
      final_metadata_files.append(f)
  sources_files = [os.path.join(constants.DIR_SOURCE_ROOT, s)
                   for s in sources_files]

  input_args = []
  for f in coverage_files + final_metadata_files:
    input_args.append('-in')
    input_args.append(f)

  output_args = ['-Dreport.html.out.file', options.output]
  source_args = ['-sp', ','.join(sources_files)]

  exit_code = cmd_helper.RunCmd(
      ['java', '-cp',
       os.path.join(constants.ANDROID_SDK_ROOT, 'tools', 'lib', 'emma.jar'),
       'emma', 'report', '-r', 'html']
      + input_args + output_args + source_args)

  if options.cleanup:
    for f in coverage_files + metadata_files:
      os.remove(f)

  if exit_code > 0:
    return exit_code
  elif err:
    return constants.WARNING_EXIT_CODE
  else:
    return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
