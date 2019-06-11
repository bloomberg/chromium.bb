# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for toolchain_util."""

from __future__ import print_function

import mock
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import toolchain_util


class GenerateChromeOrderfileTest(cros_test_lib.MockTempDirTestCase):
  """Test GenerateChromeOrderfile class."""

  # pylint: disable=protected-access
  def setUp(self):
    self.chrome_version = 'chromeos-chrome-1.0'
    self.board = 'board'
    self.out_dir = os.path.join(self.tempdir, 'outdir')
    osutils.SafeMakedirs(self.out_dir)
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    self.working_dir = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(self.working_dir)
    self.working_dir_inchroot = '/tmp'
    self.chroot_args = []

    self.test_obj = toolchain_util.GenerateChromeOrderfile(
        self.board, self.out_dir, self.chrome_version, self.chroot_dir,
        self.chroot_args)

  def testCheckArgumentsFail(self):
    """Test arguments checking fails without files existing."""
    with self.assertRaises(
        toolchain_util.GenerateChromeOrderfileError) as context:
      self.test_obj._CheckArguments()

    self.assertIn('Chrome binary does not exist at', str(context.exception))

  def testGenerateChromeNM(self):
    """Test generating chrome NM is handled correctly."""
    chrome_binary = self.test_obj.CHROME_BINARY_PATH.replace(
        '${BOARD}', self.board)
    cmd = ['llvm-nm', '-n', chrome_binary]
    output = os.path.join(self.working_dir, self.chrome_version + '.nm')
    self.test_obj.tempdir = self.tempdir
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._GenerateChromeNM()
    cros_build_lib.RunCommand.assert_called_with(
        cmd,
        log_stdout_to_file=output,
        enter_chroot=True,
        chroot_args=self.chroot_args)

  def testPostProcessOrderfile(self):
    """Test post-processing orderfile is handled correctly."""
    chrome_nm = os.path.join(self.working_dir_inchroot,
                             self.chrome_version + '.nm')
    input_orderfile = self.test_obj.INPUT_ORDERFILE_PATH.replace(
        '${BOARD}', self.board)
    output = os.path.join(self.working_dir_inchroot,
                          self.chrome_version + '.orderfile')
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._PostProcessOrderfile(chrome_nm)
    cmd = [
        self.test_obj.PROCESS_SCRIPT, '--chrome', chrome_nm, '--input',
        input_orderfile, '--output', output
    ]
    cros_build_lib.RunCommand.assert_called_with(
        cmd, enter_chroot=True, chroot_args=self.chroot_args)

  def testCreateTarball(self):
    """Test creating tarball function is handled correctly."""
    chrome_nm = os.path.join(self.working_dir, self.chrome_version + '.nm')
    input_orderfile = os.path.join(self.working_dir,
                                   self.chrome_version + '.orderfile')
    names = [self.chrome_version + '.nm', self.chrome_version + '.orderfile']
    outputs = [
        os.path.relpath(
            os.path.join(self.out_dir, n + self.test_obj.COMPRESSION_SUFFIX),
            self.working_dir) for n in names
    ]
    self.PatchObject(cros_build_lib, 'CreateTarball')
    self.test_obj._CreateTarball([chrome_nm, input_orderfile])
    calls = [
        mock.call(o, cwd=self.working_dir, inputs=[n])
        for (n, o) in zip(names, outputs)
    ]
    cros_build_lib.CreateTarball.assert_has_calls(calls)

  def testSuccessRun(self):
    """Test the main function is running successfully."""
    # Patch the two functions that generate artifacts from inputs that are
    # non-existent without actually building Chrome
    chrome_nm = os.path.join(self.working_dir, self.chrome_version + '.nm')
    osutils.Touch(chrome_nm)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_GenerateChromeNM',
        return_value=chrome_nm)
    chrome_orderfile = os.path.join(self.working_dir,
                                    self.chrome_version + '.orderfile')
    osutils.Touch(chrome_orderfile)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_PostProcessOrderfile',
        return_value=chrome_orderfile)

    self.PatchObject(toolchain_util.GenerateChromeOrderfile, '_CheckArguments')

    self.test_obj.Perform()

    # Make sure the tarballs are inside the output directory
    output_files = os.listdir(self.out_dir)
    self.assertIn(
        self.chrome_version + '.nm' + self.test_obj.COMPRESSION_SUFFIX,
        output_files)
    self.assertIn(
        self.chrome_version + '.orderfile' + self.test_obj.COMPRESSION_SUFFIX,
        output_files)
