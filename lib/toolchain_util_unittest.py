# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for toolchain_util."""

from __future__ import print_function

import collections
import mock
import os
import re

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util
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
    inputs = [chrome_nm, input_orderfile]
    compressed_names = [os.path.basename(x) for x in inputs]
    outputs = [
        os.path.join(self.out_dir,
                     n + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX)
        for n in compressed_names
    ]
    self.PatchObject(cros_build_lib, 'CompressFile')
    self.test_obj._CreateTarball(inputs)
    calls = [mock.call(n, o) for n, o in zip(inputs, outputs)]
    cros_build_lib.CompressFile.assert_has_calls(calls)

  def testSuccessRun(self):
    """Test the main function is running successfully."""
    # Patch the two functions that generate artifacts from inputs that are
    # non-existent without actually building Chrome
    chrome_nm = os.path.join(self.working_dir, self.chrome_version + '.nm')
    with open(chrome_nm, 'w') as f:
      print('Write something in the nm file', file=f)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_GenerateChromeNM',
        return_value=chrome_nm)
    chrome_orderfile = os.path.join(self.working_dir,
                                    self.chrome_version + '.orderfile')
    with open(chrome_orderfile, 'w') as f:
      print('Write something in the orderfile', file=f)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_PostProcessOrderfile',
        return_value=chrome_orderfile)

    self.PatchObject(toolchain_util.GenerateChromeOrderfile, '_CheckArguments')

    self.test_obj.Perform()

    # Make sure the tarballs are inside the output directory
    output_files = os.listdir(self.out_dir)
    self.assertIn(
        self.chrome_version + '.nm' +
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX, output_files)
    self.assertIn(
        self.chrome_version + '.orderfile' +
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX, output_files)


class UpdateChromeEbuildWithOrderfileTest(cros_test_lib.MockTempDirTestCase):
  """Test UpdateChromeEbuildWithOrderfile class."""

  # pylint: disable=protected-access
  def setUp(self):
    self.board = 'board'
    self.orderfile = 'chromeos-chrome-1.1.orderfile' + \
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX
    self.test_obj = toolchain_util.UpdateChromeEbuildWithOrderfile(
        self.board, self.orderfile)
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)

  def testFindChromeEbuild(self):
    ebuild_path = '/path/to/chromeos-chrome/chromeos-chrome-1.1.ebuild'
    mock_result = cros_build_lib.CommandResult(output=ebuild_path)
    self.PatchObject(cros_build_lib, 'RunCommand', return_value=mock_result)
    ebuild_file = self.test_obj._FindChromeEbuild()
    cmd = ['equery-%s' % self.board, 'w', 'chromeos-chrome']
    cros_build_lib.RunCommand.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, ebuild_path)

  def testPatchChromeEbuildFail(self):
    ebuild_file = os.path.join(self.tempdir, 'chromeos-chrome-1.1.ebuild')
    osutils.Touch(ebuild_file)
    with self.assertRaises(
        toolchain_util.UpdateChromeEbuildWithOrderfileError) as context:
      self.test_obj._PatchChromeEbuild(ebuild_file)

    self.assertEqual(
        'Chrome ebuild file does not have appropriate orderfile marker',
        str(context.exception))

  def testPatchChromeEbuildPass(self):
    ebuild_file = os.path.join(self.tempdir, 'chromeos-chrome-1.1.ebuild')
    with open(ebuild_file, 'w') as f:
      f.write('Some message before\n')
      f.write('UNVETTED_ORDERFILE="chromeos-chrome-1.0.orderfile"\n')
      f.write('Some message after\n')

    self.test_obj._PatchChromeEbuild(ebuild_file)
    # Make sure temporary file is removed
    self.assertNotIn('chromeos-chrome-1.1.ebuild.new', os.listdir(self.tempdir))
    # Make sure the orderfile is updated
    pattern = re.compile(self.test_obj.CHROME_EBUILD_ORDERFILE_REGEX)
    found = False
    with open(ebuild_file) as f:
      for line in f:
        matched = pattern.match(line)
        if matched:
          found = True
          self.assertEqual(
              matched.group('name'), 'chromeos-chrome-1.1.orderfile')
    self.assertTrue(found)

  def testUpdateManifest(self):
    ebuild_file = os.path.join(self.tempdir, 'chromeos-chrome-1.1.ebuild')
    cmd = ['ebuild-%s' % self.board, ebuild_file, 'manifest', '--force']
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._UpdateManifest(ebuild_file)
    cros_build_lib.RunCommand.assert_called_with(cmd, enter_chroot=True)


class CheckOrderfileExistsTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test CheckOrderfileExists command."""

  def setUp(self):
    # buildroot is not actually needed in this test
    self.buildroot = self.tempdir

  def testCheckVerifyOrderfile(self):
    """Test check orderfile for verification work properly."""
    self.PatchObject(
        toolchain_util,
        'FindLatestChromeOrderfile',
        return_value='chromeos-chrome-orderfile-1.0.tar.xz')
    for exists in [False, True]:
      self.PatchObject(gs.GSContext, 'Exists', return_value=exists)
      ret = toolchain_util.CheckOrderfileExists(
          self.buildroot, orderfile_verify=True)
      self.assertEqual(exists, ret)
      gs.GSContext.Exists.assert_called_once_with(
          os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED,
                       'chromeos-chrome-orderfile-1.0.tar.xz'))

  def testCheckGenerateOrderfile(self):
    """Test check orderfile for generation work properly."""
    unused = {
        'pv': None,
        'version_no_rev': None,
        'rev': None,
        'category': None,
        'cpv': None,
        'cp': None,
        'cpf': None
    }
    cpv = portage_util.CPV(version='1.1', package='chromeos-chrome', **unused)
    self.PatchObject(portage_util, 'PortageqBestVisible', return_value=cpv)
    for exists in [False, True]:
      self.PatchObject(gs.GSContext, 'Exists', return_value=exists)
      ret = toolchain_util.CheckOrderfileExists(
          self.buildroot, orderfile_verify=False)
      self.assertEqual(exists, ret)
      gs.GSContext.Exists.assert_called_once_with(
          toolchain_util.ORDERFILE_GS_URL_UNVETTED +
          '/chromeos-chrome-orderfile-1.1' +
          toolchain_util.ORDERFILE_COMPRESSION_SUFFIX)


class OrderfileUpdateChromeEbuildTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test OrderfileUpdateChromeEbuild and related command."""

  def setUp(self):
    self.board = 'board'
    self.unvetted_gs_url = 'gs://path/to/unvetted'
    self.vetted_gs_url = 'gs://path/to/vetted'
    MockListResult = collections.namedtuple('MockListResult',
                                            ('url', 'creation_time'))
    self.unvetted_orderfiles = [
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-2.0',
            creation_time=2.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-3.0',
            creation_time=3.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-1.0',
            creation_time=1.0)
    ]  # Intentionally unsorted
    self.PatchObject(toolchain_util, 'UpdateChromeEbuildWithOrderfile')
    self.PatchObject(
        gs.GSContext, 'List', return_value=self.unvetted_orderfiles)

  def testFindLatestChromeOrderfile(self):
    """Test FindLatestChromeOrderfile() returns latest orderfile."""
    orderfile = toolchain_util.FindLatestChromeOrderfile(self.unvetted_gs_url)
    self.assertEqual(orderfile, 'chrome-orderfile-3.0')

  def testOrderfileUpdateChromePass(self):
    ret = toolchain_util.OrderfileUpdateChromeEbuild(self.board)
    self.assertTrue(ret)
    toolchain_util.UpdateChromeEbuildWithOrderfile.assert_called_with(
        self.board, 'chrome-orderfile-3.0')
