#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

# add tools folder to sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(SCRIPT_DIR, 'tools', 'tests'))
sys.path.append(os.path.join(SCRIPT_DIR, 'build_tools', 'tests'))

TEST_MODULES = [
    'create_html_test',
    'create_nmf_test',
    'easy_template_test',
    'fix_deps_test',
    'getos_test',
    'httpd_test',
    'oshelpers_test',
    'parse_dsc_test',
    'quote_test',
    'sdktools_commands_test',
    'sdktools_config_test',
    'sdktools_test',
    'sel_ldr_test',
    'update_nacl_manifest_test',
    'verify_filelist_test',
    'verify_ppapi_test',
]

def main():
  suite = unittest.TestSuite()
  for module_name in TEST_MODULES:
    module = __import__(module_name)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))

  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
