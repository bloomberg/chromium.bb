#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for set_nacl_env.py."""

import glob
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from build_tools.sdk_tools import set_nacl_env


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_ROOT_DIR = os.path.dirname(os.path.dirname(SCRIPT_DIR))
SRC_DIR = os.path.dirname(os.path.dirname(SDK_ROOT_DIR))
NACL_DIR = os.path.join(SRC_DIR, 'native_client')


class FakeOptions(object):
  ''' Just a placeholder for options '''
  pass


class TestSetNaclEnv(unittest.TestCase):
  ''' Test basic functionality of the set_nacl_env package '''
  def setUp(self):
    self._options = FakeOptions()
    self._options.host = 'mac'
    self._options.lib_variant = 'newlib'
    self._options.sdk_root = NACL_DIR
    self._options.sdk_platform = 'pepper_17'
    self._options.build_type = 'debug'
    self._options.no_ppapi = False
    self._options.merge = False

    self._env = {}
    self._env['NACL_SDK_ROOT'] = NACL_DIR

    self._temp_dir = tempfile.mkdtemp(prefix='tmp_set_nacl_env_test')

  def tearDown(self):
    shutil.rmtree(self._temp_dir, ignore_errors=True)

  def testBuildEnvX86_32(self):
    ''' Test setting up a x86_32 build env '''
    self._options.arch = 'x86-32'
    self._options.toolchain_path = set_nacl_env.GetToolchainPath(self._options)
    env = set_nacl_env.SetupX86Env(self._options)
    # Verify a few essential build options.
    self.assertTrue('CFLAGS' in env)
    self.assertTrue('CC' in env)
    self.assertTrue('-m32' in env['CC'])
    self.assertTrue('pepper_17' in env['CC'])
    self.assertTrue('CXX' in env)
    self.assertTrue('-m32' in env['CXX'])

  def testBuildEnvX86_64(self):
    ''' Test setting up a x86_64 build env '''
    self._options.arch = 'x86-64'
    self._options.toolchain_path = set_nacl_env.GetToolchainPath(self._options)
    env = set_nacl_env.SetupX86Env(self._options)
    # Verify a few essential build options.
    self.assertTrue('CFLAGS' in env)
    self.assertTrue('CC' in env)
    self.assertTrue('-m64' in env['CC'])
    self.assertTrue('pepper_17' in env['CC'])
    self.assertTrue('CXX' in env)
    self.assertTrue('-m64' in env['CXX'])

  def testBuildWithMake(self):
    ''' Test building hello_world_c with make '''
    def MakeClean():
      ''' Invoke 'make clean' in the current directory and check the outcome.
      '''
      cmd = [script, 'make clean --silent']
      self.assertEqual(0, subprocess.call(cmd, env=self._env, shell=False,
                                          cwd=self._temp_dir))
      self.assertFalse(os.path.exists('hello_world_c.nexe'))

    # Can't use make on Windows
    if sys.platform == 'win32':
      return

    # The test directories are not generally writable. To be able to run make,
    # we copy the files to a temp directory instead.
    self.assertTrue(self._temp_dir)
    src_dir = os.path.join(SCRIPT_DIR, 'set_nacl_env_test_archive')
    for file in glob.iglob(os.path.join(src_dir, '*')):
      shutil.copy2(file, self._temp_dir)

    script = os.path.join(SDK_ROOT_DIR, 'build_tools', 'sdk_tools',
                          'set_nacl_env.py')
    nexe_path = os.path.join(self._temp_dir, 'hello_world_c.nexe')

    # Build and verify the 32-bit version.
    options = ['--platform=.', '--arch=x86-32']
    cmd = [script] + options + ['make hello_world_c.nexe']
    self.assertEqual(0, subprocess.call(cmd, env=self._env, shell=False,
                                        cwd=self._temp_dir))
    self.assertTrue(os.path.exists(nexe_path))
    size_32 = os.path.getsize(nexe_path)
    MakeClean()

    # Build and verify the 64-bit version.
    options = ['--platform=.', '--arch=x86-64']
    cmd = [script] + options + ['make hello_world_c.nexe']
    self.assertEqual(0, subprocess.call(cmd, env=self._env, shell=False,
                                        cwd=self._temp_dir))
    self.assertTrue(os.path.exists(nexe_path))
    size_64 = os.path.getsize(nexe_path)
    MakeClean()

    # Verify that 64-bit version of nexe is larger than 32-bit version.
    self.assertTrue(size_64 > size_32)


def main():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestSetNaclEnv)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(main())
