#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)
DATA_DIR = os.path.join(SCRIPT_DIR, 'data')

sys.path.append(PARENT_DIR)

import getos
import create_nmf


class TestIsDynamicElf(unittest.TestCase):
  def test_arm(self):
    static_nexe = os.path.join(DATA_DIR, 'test_static_arm.nexe')
    self.assertFalse(create_nmf.IsDynamicElf(static_nexe, False))

  def test_x86_32(self):
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_32.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')
    self.assertTrue(create_nmf.IsDynamicElf(dyn_nexe, False))
    self.assertFalse(create_nmf.IsDynamicElf(static_nexe, False))

  def test_x86_64(self):
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_64.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_64.nexe')
    self.assertTrue(create_nmf.IsDynamicElf(dyn_nexe, True))
    self.assertFalse(create_nmf.IsDynamicElf(static_nexe, True))


class TestParseElfHeader(unittest.TestCase):
  def test_invalid_elf(self):
    self.assertRaises(create_nmf.Error, create_nmf.ParseElfHeader, __file__)

  def test_arm_elf_parse(self):
    """Test parsing of ARM elf header."""
    static_nexe = os.path.join(DATA_DIR, 'test_static_arm.nexe')
    arch, dynamic = create_nmf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'arm')
    self.assertFalse(dynamic)

  def test_x86_32_elf_parse(self):
    """Test parsing of x86-32 elf header."""
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_32.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')

    arch, dynamic = create_nmf.ParseElfHeader(dyn_nexe)
    self.assertEqual(arch, 'x86-32')
    self.assertTrue(dynamic)

    arch, dynamic = create_nmf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'x86-32')
    self.assertFalse(dynamic)

  def test_x86_64_elf_parse(self):
    """Test parsing of x86-64 elf header."""
    dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_64.nexe')
    static_nexe = os.path.join(DATA_DIR, 'test_static_x86_64.nexe')

    arch, dynamic = create_nmf.ParseElfHeader(dyn_nexe)
    self.assertEqual(arch, 'x86-64')
    self.assertTrue(dynamic)

    arch, dynamic = create_nmf.ParseElfHeader(static_nexe)
    self.assertEqual(arch, 'x86-64')
    self.assertFalse(dynamic)


class TestNmfUtils(unittest.TestCase):
  """Tests for the main NmfUtils class in create_nmf."""

  def setUp(self):
    self.tempdir = None
    chrome_src = os.path.dirname(os.path.dirname(os.path.dirname(PARENT_DIR)))
    toolchain = os.path.join(chrome_src, 'native_client', 'toolchain')
    self.toolchain = os.path.join(toolchain, '%s_x86' % getos.GetPlatform())
    self.objdump = os.path.join(self.toolchain, 'bin', 'i686-nacl-objdump')
    if os.name == 'nt':
      self.objdump += '.exe'
    self.Mktemp()
    self.dyn_nexe = self.createTestNexe('test_dynamic_x86_32.nexe', True,
                                        'i686')
    self.dyn_deps = set(['libc.so', 'runnable-ld.so',
                         'libgcc_s.so', 'libpthread.so'])

  def createTestNexe(self, name, dynamic, arch):
    """Create an empty test .nexe file for use in create_nmf tests.

    This is used rather than checking in test binaries since the
    checked in binaries depend on .so files that only exist in the
    certain SDK that build them.
    """
    compiler = os.path.join(self.toolchain, 'bin', '%s-nacl-g++' % arch)
    if os.name == 'nt':
      compiler += '.exe'
      os.environ['CYGWIN'] = 'nodosfilewarning'
    program = 'int main() { return 0; }'
    name = os.path.join(self.tempdir, name)
    cmd = [compiler, '-pthread', '-x' , 'c', '-o', name, '-']
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    p.communicate(input=program)
    self.assertEqual(p.returncode, 0)
    return name

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def Mktemp(self):
    self.tempdir = tempfile.mkdtemp()

  def CreateNmfUtils(self):
    libdir = os.path.join(self.toolchain, 'x86_64-nacl', 'lib32')
    return create_nmf.NmfUtils([self.dyn_nexe],
                               lib_path=[libdir],
                               objdump=self.objdump)

  def testGetNeededStatic(self):
    nexe = os.path.join(DATA_DIR, 'test_static_x86_32.nexe')
    nmf = create_nmf.NmfUtils([nexe])
    needed = nmf.GetNeeded()

    # static nexe should have exactly one needed file
    self.assertEqual(len(needed), 1)
    self.assertEqual(needed.keys()[0], nexe)

    # arch of needed file should be x86-32
    archfile = needed.values()[0]
    self.assertEqual(archfile.arch, 'x86-32')

  def StripDependencies(self, deps):
    """Strip the dirnames and version suffixes from
    a list of nexe dependencies.

    e.g:
    /path/to/libpthread.so.1a2d3fsa -> libpthread.so
    """
    names = []
    for name in deps:
      name = os.path.basename(name)
      if '.so.' in name:
        name = name.rsplit('.', 1)[0]
      names.append(name)
    return names

  def testGetNeededDynamic(self):
    nmf = self.CreateNmfUtils()
    needed = nmf.GetNeeded()
    names = needed.keys()

    # this nexe has 5 dependencies
    expected = set(self.dyn_deps)
    expected.add(os.path.basename(self.dyn_nexe))

    basenames = set(self.StripDependencies(names))
    self.assertEqual(expected, basenames)

  def testStageDependencies(self):
    self.Mktemp()
    nmf = self.CreateNmfUtils()

    # Stage dependencies
    nmf.StageDependencies(self.tempdir)

    # Verify directory contents
    contents = set(os.listdir(self.tempdir))
    expectedContents = set((os.path.basename(self.dyn_nexe), 'lib32'))
    self.assertEqual(contents, expectedContents)

    contents = os.listdir(os.path.join(self.tempdir, 'lib32'))
    contents = self.StripDependencies(contents)
    contents = set(contents)
    expectedContents = self.dyn_deps
    self.assertEqual(contents, expectedContents)


if __name__ == '__main__':
  unittest.main()
