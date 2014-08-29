#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
import unittest

# add tools folder to sys.path
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.join(SCRIPT_DIR, 'tools')
BUILD_TOOLS_DIR = os.path.join(SCRIPT_DIR, 'build_tools')

sys.path.append(TOOLS_DIR)
sys.path.append(os.path.join(TOOLS_DIR, 'tests'))
sys.path.append(os.path.join(TOOLS_DIR, 'lib', 'tests'))
sys.path.append(BUILD_TOOLS_DIR)
sys.path.append(os.path.join(BUILD_TOOLS_DIR, 'tests'))

import build_paths

PKG_VER_DIR = os.path.join(build_paths.NACL_DIR, 'build', 'package_version')
TAR_DIR = os.path.join(build_paths.NACL_DIR, 'toolchain', '.tars')

PKG_VER = os.path.join(PKG_VER_DIR, 'package_version.py')

EXTRACT_PACKAGES = ['nacl_x86_glibc']
TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')

TEST_MODULES = [
    'build_version_test',
    'create_html_test',
    'create_nmf_test',
    'easy_template_test',
    'elf_test',
    'fix_deps_test',
    'getos_test',
    'get_shared_deps_test',
    'httpd_test',
    'nacl_config_test',
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

def ExtractToolchains():
  subprocess.check_output([sys.executable, PKG_VER,
                           '--packages', ','.join(EXTRACT_PACKAGES),
                           '--tar-dir', TAR_DIR,
                           '--dest-dir', TOOLCHAIN_OUT,
                           'extract'])

def main():
  # Some of the unit tests use parts of toolchains. Extract to TOOLCHAIN_OUT.
  ExtractToolchains()

  suite = unittest.TestSuite()
  for module_name in TEST_MODULES:
    module = __import__(module_name)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))

  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
