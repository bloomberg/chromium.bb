#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import re
import sys


_HERE_PATH = os.path.join(os.path.dirname(__file__))

# The name of a dummy file to be updated always after all other files have been
# written. This file is declared as the "output" for GN's purposes
_TIMESTAMP_FILENAME = os.path.join('unpack.stamp')


_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
sys.path.insert(1, os.path.join(_SRC_PATH, 'tools', 'grit'))
from grit.format import data_pack


def unpack(pak_path, out_path):
  pak_dir = os.path.dirname(pak_path)
  pak_id = os.path.splitext(os.path.basename(pak_path))[0]

  data = data_pack.DataPack.ReadDataPack(pak_path)

  # Associate numerical grit IDs to strings.
  # For example 120045 -> 'IDR_SETTINGS_ABOUT_PAGE_HTML'
  resource_ids = dict()
  resources_path = os.path.join(pak_dir, 'grit', pak_id + '.h')
  with open(resources_path) as resources_file:
    for line in resources_file:
      res = re.match('#define ([^ ]+) (\d+)', line)
      if res:
        resource_ids[int(res.group(2))] = res.group(1)
  assert resource_ids

  # Associate numerical string IDs to files.
  resource_filenames = dict()
  resources_map_path = os.path.join(pak_dir, 'grit', pak_id + '_map.cc')
  with open(resources_map_path) as resources_map:
    for line in resources_map:
      res = re.match('  {"([^"]+)", ([^}]+)', line)
      if res:
        resource_filenames[res.group(2)] = res.group(1)
  assert resource_filenames

  # Extract packed files, while preserving directory structure.
  for (resource_id, text) in data.resources.iteritems():
    filename = resource_filenames[resource_ids[resource_id]]
    dirname = os.path.join(out_path, os.path.dirname(filename))
    if not os.path.exists(dirname):
      os.makedirs(dirname)
    with open(os.path.join(out_path, filename), 'w') as file:
      file.write(text)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--pak_file')
  parser.add_argument('--out_folder')
  args = parser.parse_args()

  unpack(args.pak_file, args.out_folder)

  timestamp_file_path = os.path.join(args.out_folder, _TIMESTAMP_FILENAME)
  with open(timestamp_file_path, 'a'):
    os.utime(timestamp_file_path, None)


if __name__ == '__main__':
  main()
