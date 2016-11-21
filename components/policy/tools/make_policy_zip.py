#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a zip archive with policy template files. The list of input files is
extracted from a grd file with grit. This is to keep the length of input
arguments below the limit on Windows.
"""

import grd_helper
import optparse
import os
import sys
import zipfile


def add_files_to_zip(zip_file, base_dir, file_list):
  """Pack a list of files into a zip archive, that is already
  opened for writing.

  Args:
    zip_file: An object representing the zip archive.
    base_dir: Base path of all the files in the real file system.
    files: List of file paths to add, all relative to base_dir.
        The zip entries will only contain this componenet of the path.
  """
  for file_path in file_list:
    zip_file.write(base_dir + file_path, file_path)
  return 0


def main(argv):
  """Pack a list of files into a zip archive.

  Args:
    zip_path: The file name of the zip archive.
    base_dir: Base path of input files.
    locales: The list of locales that are used to generate the list of file
        names using INPUT_FILES.
  """
  parser = optparse.OptionParser()
  parser.add_option("--output", dest="output")
  parser.add_option("--basedir", dest="basedir")
  parser.add_option("--include_google_admx", action="store_true",
                    dest="include_google_admx", default=False)
  parser.add_option("--extra_input", action="append", dest="extra_input",
                    default=[])
  grd_helper.add_options(parser)
  options, args = parser.parse_args(argv[1:])

  if (options.basedir[-1] != '/'):
    options.basedir += '/'

  file_list = options.extra_input
  file_list += grd_helper.get_grd_outputs(options)

  # Pick up google.admx/adml files.
  if (options.include_google_admx):
    google_file_list = []
    for path in file_list:
      directory, filename = os.path.split(path)
      filename, extension = os.path.splitext(filename)
      if extension == ".admx" or extension == ".adml":
        google_file_list.append(\
            os.path.join(options.basedir, directory, "google" + extension))
    file_list.extend(google_file_list)

  zip_file = zipfile.ZipFile(options.output, 'w', zipfile.ZIP_DEFLATED)
  try:
    return add_files_to_zip(zip_file, options.basedir, file_list)
  finally:
    zip_file.close()


if '__main__' == __name__:
  sys.exit(main(sys.argv))
