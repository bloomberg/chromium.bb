#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import unittest

TEST_MODULES = [
    'test_auto_update_sdktools',
    'test_update_nacl_manifest'
]

def main():
  suite = unittest.TestSuite()
  for module_name in TEST_MODULES:
    __import__(module_name)
    module = sys.modules[module_name]
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))

  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
