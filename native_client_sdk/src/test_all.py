#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tarfile
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
import getos

TOOLCHAIN_OUT = os.path.join(build_paths.OUT_DIR, 'sdk_tests', 'toolchain')
NACL_X86_GLIBC_TOOLCHAIN = os.path.join(TOOLCHAIN_OUT, 'nacl_x86_glibc')

TEST_MODULES = [
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

def ExtractToolchain():
  # TODO(dyen): This function is basically all temporary code. After the DEPS
  # roll, replace this untar with package_version.py which will do the
  # untarring and be able to check whether or not the toolchain is out of date.
  platform_name = getos.GetPlatform()
  nacl_x86_glibc_tar = os.path.join(build_paths.NACL_DIR, 'toolchain', '.tars',
                                    'toolchain_%s_x86.tar.bz2' % platform_name)
  assert os.path.isfile(nacl_x86_glibc_tar), 'Could not locate toolchain tar'

  stamp_file = os.path.join(NACL_X86_GLIBC_TOOLCHAIN, 'extract.stamp')
  if (not os.path.isfile(stamp_file) or
      os.path.getmtime(stamp_file) < os.path.getmtime(nacl_x86_glibc_tar)):

    print 'Extracting nacl_x86_glibc...'
    temp_dir = os.path.join(TOOLCHAIN_OUT, 'temp')
    if os.path.isdir(temp_dir):
      shutil.rmtree(temp_dir)
    os.makedirs(temp_dir)
    with tarfile.open(nacl_x86_glibc_tar) as f:
      f.extractall(temp_dir)

    if os.path.isdir(NACL_X86_GLIBC_TOOLCHAIN):
      shutil.rmtree(NACL_X86_GLIBC_TOOLCHAIN)
    parent_dir = os.path.dirname(NACL_X86_GLIBC_TOOLCHAIN)
    if not os.path.isdir(parent_dir):
      os.makedirs(parent_dir)

    extracted_path = os.path.join(temp_dir, 'toolchain',
                                  '%s_x86' % platform_name)
    os.rename(extracted_path, NACL_X86_GLIBC_TOOLCHAIN)

    with open(stamp_file, 'wt') as f:
      f.write(nacl_x86_glibc_tar)

def main():
  # Some of the unit tests utilize nacl_x86_glibc. Untar a copy in the output
  # directory for the tests to use.
  ExtractToolchain()

  suite = unittest.TestSuite()
  for module_name in TEST_MODULES:
    module = __import__(module_name)
    suite.addTests(unittest.defaultTestLoader.loadTestsFromModule(module))

  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return int(not result.wasSuccessful())

if __name__ == '__main__':
  sys.exit(main())
