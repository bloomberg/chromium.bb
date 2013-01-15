#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import shutil
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
    self.dyn_nexe = os.path.join(DATA_DIR, 'test_dynamic_x86_32.nexe')
    self.dyn_deps = set(['libc.so.51fe1ff9', 'runnable-ld.so',
                         'libgcc_s.so.1', 'libpthread.so.51fe1ff9'])

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

  def testGetNeededDynamic(self):
    nmf = self.CreateNmfUtils()
    needed = nmf.GetNeeded()
    names = needed.keys()

    # this nexe has 5 dependancies
    expected = set(self.dyn_deps)
    expected.add(os.path.basename(self.dyn_nexe))

    basenames = set(os.path.basename(n) for n in names)
    self.assertEqual(expected, basenames)

  def testStageDependencies(self):
    self.Mktemp()
    nmf = self.CreateNmfUtils()

    # Stage dependancies
    nmf.StageDependencies(self.tempdir)

    # Verify directory contents
    contents = set(os.listdir(self.tempdir))
    expectedContents = set((os.path.basename(self.dyn_nexe), 'lib32'))
    self.assertEqual(contents, expectedContents)

    contents = set(os.listdir(os.path.join(self.tempdir, 'lib32')))
    expectedContents = self.dyn_deps
    self.assertEqual(contents, expectedContents)


if __name__ == '__main__':
  unittest.main()
