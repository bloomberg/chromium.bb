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

def main():
  shutil.rmtree(LOCAL_THIRD_PARTY_DIR, True)
  shutil.copytree(os.path.join(THIRD_PARTY_DIR, 'handlebar'),
      os.path.join(LOCAL_THIRD_PARTY_DIR, 'handlebar'))

  with open(os.path.join(LOCAL_THIRD_PARTY_DIR, '__init__.py'), 'w') as f:
    os.utime(os.path.join(LOCAL_THIRD_PARTY_DIR, '__init__.py'), None)

  shutil.copy(os.path.join(LOCAL_THIRD_PARTY_DIR, '__init__.py'),
      os.path.join(LOCAL_THIRD_PARTY_DIR, 'handlebar'))
  with open(
      os.path.join(LOCAL_THIRD_PARTY_DIR, 'handlebar/__init__.py'), 'a') as f:
    f.write('\nfrom handlebar import Handlebar\n')

if __name__ == '__main__':
  main()
