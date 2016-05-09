# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import shutil
import sys


def detect_encoding(data, default_encoding='UTF-8'):
  """Detects the encoding used by |data| from the Byte-Order-Mark if present.

  Args:
    data: string whose encoding needs to be detected
    default_encoding: encoding returned if no BOM is found.

  Returns:
    The encoding determined from the BOM if present or |default_encoding| if
    no BOM was found.
  """
  if data.startswith('\xFE\xFF'):
    return 'UTF-16BE'

  if data.startswith('\xFF\xFE'):
    return 'UTF-16LE'

  if data.startswith('\xEF\xBB\xBF'):
    return 'UTF-8'

  return default_encoding


def copy_strings_file(source, dest):
  """Copies a .strings file from |source| to |dest| and convert it to UTF-16.

  Args:
    source: string, path to the source file
    dest: string, path to the destination file
  """
  with open(source, 'rb') as source_file:
    data = source_file.read()

  # Xcode's CpyCopyStringsFile / builtin-copyStrings seems to call
  # CFPropertyListCreateFromXMLData() behind the scenes; at least it prints
  #     CFPropertyListCreateFromXMLData(): Old-style plist parser: missing
  #     semicolon in dictionary.
  # on invalid files. Do the same kind of validation.
  from CoreFoundation import CFDataCreate, CFPropertyListCreateFromXMLData
  cfdata = CFDataCreate(None, data, len(data))
  _, error = CFPropertyListCreateFromXMLData(None, cfdata, 0, None)
  if error:
    raise ValueError(error)

  encoding = detect_encoding(data)
  with open(dest, 'wb') as dest_file:
    dest_file.write(data.decode(encoding).encode('UTF-16'))


def copy_file(source, dest):
  """Copies a file or directory from |source| to |dest|.

  Args:
    source: string, path to the source file
    dest: string, path to the destination file
  """
  if os.path.isdir(source):
    if os.path.exists(dest):
      shutil.rmtree(dest)
    # Copy tree.
    # TODO(thakis): This copies file attributes like mtime, while the
    # single-file branch below doesn't. This should probably be changed to
    # be consistent with the single-file branch.
    shutil.copytree(source, dest)
    return

  if os.path.exists(dest):
    os.unlink(dest)

  _, extension = os.path.splitext(source)
  if extension == '.strings':
    copy_strings_file(source, dest)
    return

  shutil.copy(source, dest)


def main():
  parser = argparse.ArgumentParser(
      description='copy source to destination for the creation of a bundle')
  parser.add_argument('source', help='path to source file or directory')
  parser.add_argument('dest', help='path to destination')
  args = parser.parse_args()

  copy_file(args.source, args.dest)

if __name__ == '__main__':
  main()
