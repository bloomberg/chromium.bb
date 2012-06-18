#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is used to copy all dependencies into the local directory.
# The package of files can then be uploaded to App Engine.
import os
import shutil
import sys

THIRD_PARTY_DIR = os.path.join(sys.path[0], os.pardir, os.pardir, os.pardir,
     os.pardir, os.pardir, 'third_party')
LOCAL_THIRD_PARTY_DIR = os.path.join(sys.path[0], 'third_party')
TOOLS_DIR = os.path.join(sys.path[0], os.pardir, os.pardir, os.pardir,
     os.pardir, os.pardir, 'tools')

SCHEMA_COMPILER_FILES = ['model.py']

def MakeInit(path):
  path = os.path.join(path, '__init__.py')
  with open(os.path.join(path), 'w') as f:
    os.utime(os.path.join(path), None)

def CopyThirdParty(src, dest, files=None):
  dest_path = os.path.join(LOCAL_THIRD_PARTY_DIR, dest)
  if not files:
    shutil.copytree(src, dest_path)
    MakeInit(dest_path)
    return
  try:
    os.makedirs(dest_path)
  except:
    pass
  MakeInit(dest_path)
  for filename in files:
    shutil.copy(os.path.join(src, filename), os.path.join(dest_path, filename))

def main():
  shutil.rmtree(LOCAL_THIRD_PARTY_DIR, True)

  CopyThirdParty(os.path.join(THIRD_PARTY_DIR, 'handlebar'), 'handlebar')
  CopyThirdParty(os.path.join(TOOLS_DIR, 'json_schema_compiler'),
                 'json_schema_compiler',
                 SCHEMA_COMPILER_FILES)
  CopyThirdParty(TOOLS_DIR, 'json_schema_compiler', ['json_comment_eater.py'])
  MakeInit(LOCAL_THIRD_PARTY_DIR)

  # To be able to use the Handlebar class we need this import in __init__.py.
  with open(os.path.join(LOCAL_THIRD_PARTY_DIR,
                         'handlebar',
                         '__init__.py'), 'a') as f:
    f.write('from handlebar import Handlebar\n')

if __name__ == '__main__':
  main()
