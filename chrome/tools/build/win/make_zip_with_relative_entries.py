# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a zip archive with relative entry paths, from a list of files with
absolute paths.
"""

import sys
import zipfile

def add_files_to_zip(zip_file, base_dir, files):
  """Pack a list of files into a zip archive, that is already
  opened for writing.

  Args:
    zip_file: An object representing the zip archive.
    base_dir: A path that will be stripped from the beginning of the paths of
      added files.
    files: The list of file paths to add.
  """
  prefix_len = len(base_dir)

  for file_path in files:
    if not file_path.startswith(base_dir):
      print "All the input files should be in the base directory."
      return 1
    entry_path = file_path[prefix_len:]
    zip_file.write(file_path, entry_path)

  return 0


def main(zip_path, base_dir, files):
  """Pack a list of files into a zip archive.

  Args:
    zip_path: The file name of the zip archive.
    base_dir: A path that will be stripped from the beginning of the paths of
      added files.
    files: The list of file paths to add.

  Example:
    main('x.zip', '/a/b', ['/a/b/c/1.txt', '/a/b/d/2.txt'])
    Will put the following entries to x.zip:
      c/1.txt
      d/2.txt
  """
  if (base_dir[-1] != '/'):
    base_dir = base_dir + '/'

  zip_file = zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED)
  try:
    return add_files_to_zip(zip_file, base_dir, files)
  finally:
    zip_file.close()

if '__main__' == __name__:
  if len(sys.argv) < 4:
    print "usage: %s output.zip path/to/base/dir list of files" % sys.argv[0]
    sys.exit(1)

  sys.exit(main(sys.argv[1], sys.argv[2], sys.argv[3:]))
