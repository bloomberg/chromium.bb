#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a zip archive with policy template files. The list of input files is
extracted from a grd file with grit. This is to keep the length of input
arguments below the limit on Windows.
"""

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


def get_grd_outputs(grit_cmd, grit_defines, grd_file, grd_strip_path_prefix):
  grit_path = os.path.join(os.getcwd(), os.path.dirname(grit_cmd))
  sys.path.append(grit_path)
  import grit_info
  outputs = grit_info.Outputs(grd_file, grit_defines,
                              'GRIT_DIR/../gritsettings/resource_ids')
  result = []
  for item in outputs:
    assert item.startswith(grd_strip_path_prefix)
    result.append(item[len(grd_strip_path_prefix):])
  return result


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
  parser.add_option("--grit_info", dest="grit_info")
  parser.add_option("--grd_input", dest="grd_input")
  parser.add_option("--grd_strip_path_prefix", dest="grd_strip_path_prefix")
  parser.add_option("--extra_input", action="append", dest="extra_input",
                    default=[])
  parser.add_option("-D", action="append", dest="grit_defines", default=[])
  parser.add_option("-E", action="append", dest="grit_build_env", default=[])
  options, args = parser.parse_args(argv[1:])

  if (options.basedir[-1] != '/'):
    options.basedir += '/'
  grit_defines = {}
  for define in options.grit_defines:
    grit_defines[define] = 1

  file_list = options.extra_input
  file_list += get_grd_outputs(options.grit_info, grit_defines,
                               options.grd_input, options.grd_strip_path_prefix)
  zip_file = zipfile.ZipFile(options.output, 'w', zipfile.ZIP_DEFLATED)
  try:
    return add_files_to_zip(zip_file, options.basedir, file_list)
  finally:
    zip_file.close()


if '__main__' == __name__:
  sys.exit(main(sys.argv))
