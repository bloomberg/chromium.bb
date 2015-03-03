# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os.path
import re

def CreateArgumentParser():
  parser = argparse.ArgumentParser()
  parser.add_argument('declaration', help='Path to declaration file.')
  parser.add_argument('--root', default='.', help='Path to src/ dir.')
  parser.add_argument('--destination',
                      help='Root directory for generated files.')
  return parser

def ToUpperCamelCase(underscore_name):
  return re.sub(r'(?:_|^)(.)', lambda m: m.group(1).upper(), underscore_name)

def ToLowerCamelCase(underscore_name):
  return re.sub(r'_(.)', lambda m: m.group(1).upper(), underscore_name)

def PathToIncludeGuard(path):
  return re.sub(r'[/.]', '_', path.upper()) + '_'

def CreateDirIfNotExists(path):
  if not os.path.isdir(path):
    if os.path.exists(path):
      raise Exception('%s exists and is not a directory.' % path)
    os.makedirs(path)
