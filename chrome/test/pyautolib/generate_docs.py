#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import pydoc
import shutil
import sys


def main():
  parser = optparse.OptionParser()
  parser.add_option('-w', '--write', dest='dir', metavar='FILE',
                    default=os.path.join(os.getcwd(), 'pyauto_docs'),
                    help=('Directory path to write all of the documentation. '
                    'Defaults to "pyauto_docs" in current directory.'))
  parser.add_option('-p', '--pyautolib', dest='pyautolib', metavar='FILE',
                    default=os.getcwd(),
                    help='Location of pyautolib directory')
  (options, args) = parser.parse_args()

  if not os.path.isdir(options.dir):
    os.makedirs(options.dir)

  # Add these paths so pydoc can find everything
  sys.path.append(os.path.join(options.pyautolib,
                  '../../../third_party/'))
  sys.path.append(options.pyautolib)

  # Get a snapshot of the current directory where pydoc will export the files
  previous_contents = set(os.listdir(os.getcwd()))
  pydoc.writedocs(options.pyautolib)
  current_contents = set(os.listdir(os.getcwd()))

  if options.dir == os.getcwd():
    print 'Export complete, files are located in %s' % options.dir
    return 1

  new_files = current_contents.difference(previous_contents)
  for file_name in new_files:
    basename, extension = os.path.splitext(file_name)
    if extension == '.html':
      # Build the complete path
      full_path = os.path.join(os.getcwd(), file_name)
      existing_file_path = os.path.join(options.dir, file_name)
      if os.path.isfile(existing_file_path):
        os.remove(existing_file_path)
      shutil.move(full_path, options.dir)

  print 'Export complete, files are located in %s' % options.dir
  return 0


if __name__ == '__main__':
  sys.exit(main())
