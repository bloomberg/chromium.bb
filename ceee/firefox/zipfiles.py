# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Compresses input arguments to a zip archive."""

import optparse
import os
import shutil
import sys
import zipfile


# Special token to separate the regular files needed in the XPI file from
# the debug-only files.  The regular files are listed first, then the separator,
# followed by the debug only files.
_DEBUG_ONLY_FILE_SEPARATOR = '##'


# Usage string for option parser.
_USAGE='Usage: %prog [options] <file-list> [## <debug-only-file-list>]'


# Help description text for option parser.
_DESCRIPTION="""Creates a zip file containing the list of files specified on
the command line.

The files before the ## token are always added to the zip file.  The files
after the ## token are added only if the configuration specified is Debug.

If relative paths are used to name the files, they are assumed to be relative
to the current working directory.  Any directory structure will be preserved
unless the -p command line option is used to place the files in a specific
folder of the zip file.
"""


def ParseCommandLine():
  """Constructs and configures an option parser for this script and parses
  the command line arguments.

  Returns:
    The parsed options.
  """
  parser = optparse.OptionParser(usage=_USAGE, description=_DESCRIPTION)
  parser.add_option('-i', '--input', dest='input',
                    help='An archive file to append to.')
  parser.add_option('-o', '--output', dest='output',
                    help='Output file for archive.')
  parser.add_option('-p', '--path', dest='path',
                    help='Path within the archive to write the files to.')
  parser.add_option('-c', '--config', dest='config',
                    help='Configuration being built, either Debug or Release.')

  (options, args) = parser.parse_args()

  if not options.output:
    parser.error('You must provide an output file.')

  if not options.config:
    parser.error('You must provide configuration.')

  return (options, args)


def Main():
  """Zip input arguments to Zip output archive."""
  (options, args) = ParseCommandLine()

  # Test that all the input files exist before we blow the output archive away
  # only to fail part way.
  for path in args:
    if path == _DEBUG_ONLY_FILE_SEPARATOR:
      continue
    if not os.path.exists(path):
      raise Exception('Input file does not exist:', path)
    if os.path.isdir(path):
      raise Exception('Input file is a path:', path)

  # Open the output file.
  if options.input:
    shutil.copyfile(options.input, options.output)
    mode = 'a'
  else:
    mode = 'w'
  output = zipfile.ZipFile(options.output, mode, zipfile.ZIP_DEFLATED)

  # Add the files to the output archive.
  for path in args:
    if path == _DEBUG_ONLY_FILE_SEPARATOR:
      if options.config != 'Debug':
        break
    elif options.path:
      output.write(path, os.path.join(options.path, os.path.basename(path)))
    else:
      output.write(path)

  output.close()

  return 0


if __name__ == '__main__':
  sys.exit(Main())
