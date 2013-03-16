# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import shlex
import shutil


def MakeDirectory(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


def DeleteDirectory(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def Touch(path):
  MakeDirectory(os.path.dirname(path))
  with open(path, 'a'):
    os.utime(path, None)


def FindInDirectory(directory, filter):
  files = []
  for root, dirnames, filenames in os.walk(directory):
    matched_files = fnmatch.filter(filenames, filter)
    files.extend((os.path.join(root, f) for f in matched_files))
  return files


def FindInDirectories(directories, filter):
  all_files = []
  for directory in directories:
    all_files.extend(FindInDirectory(directory, filter))
  return all_files


def ParseGypList(gyp_string):
  # The ninja generator doesn't support $ in strings, so use ## to
  # represent $.
  # TODO(cjhopman): Remove when
  # https://code.google.com/p/gyp/issues/detail?id=327
  # is addressed.
  gyp_string = gyp_string.replace('##', '$')
  return shlex.split(gyp_string)


