# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for toolchain_util."""

from __future__ import print_function

import collections
import io
import json
import os
import re
import time

import mock

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import timeout_util
from chromite.lib import toolchain_util


class ProfilesNameHelperTest(cros_test_lib.MockTempDirTestCase):
  """Test the helper functions related to naming."""

  # pylint: disable=protected-access
  def testParseBenchmarkProfileName(self):
    """Test top-level function _ParseBenchmarkProfileName."""
    # Test parse failure
    profile_name_to_fail = 'this_is_an_invalid_name'
    with self.assertRaises(toolchain_util.ProfilesNameHelperError) as context:
      toolchain_util._ParseBenchmarkProfileName(profile_name_to_fail)
    self.assertIn('Unparseable benchmark profile name:', str(context.exception))

    # Test parse success
    profile_name = 'chromeos-chrome-amd64-77.0.3849.0_rc-r1.afdo'
    result = toolchain_util._ParseBenchmarkProfileName(profile_name)
    self.assertEqual(
        result,
        toolchain_util.BenchmarkProfileVersion(
            major=77, minor=0, build=3849, patch=0, revision=1,
            is_merged=False))

  def testParseCWPProfileName(self):
    """Test top-level function _ParseCWPProfileName."""
    # Test parse failure
    profile_name_to_fail = 'this_is_an_invalid_name'
    with self.assertRaises(toolchain_util.ProfilesNameHelperError) as context:
      toolchain_util._ParseCWPProfileName(profile_name_to_fail)
    self.assertIn('Unparseable CWP profile name:', str(context.exception))

    # Test parse success
    profile_name = 'R77-3809.38-1562580965.afdo.xz'
    result = toolchain_util._ParseCWPProfileName(profile_name)
    self.assertEqual(
        result,
        toolchain_util.CWPProfileVersion(
            major=77, build=3809, patch=38, clock=1562580965))

  def testGetArtifactVersionInEbuild(self):
    """Test top-level function _GetArtifactVersionInEbuild."""
    package = 'package'
    ebuild_file = os.path.join(self.tempdir, 'package.ebuild')
    variables = ['variable_name', 'another_variable_name']
    values = ['old-afdo-artifact-1.0', 'another-old-afdo-artifact-1.0']
    ebuild_file_content = '\n'.join([
        'Some message before',
        '%s="%s"' % (variables[0], values[0]),
        '%s="%s"' % (variables[1], values[1]), 'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)
    self.PatchObject(
        toolchain_util, '_FindEbuildPath', return_value=ebuild_file)
    for n, v in zip(variables, values):
      ret = toolchain_util._GetArtifactVersionInEbuild(package, n)
      self.assertEqual(ret, v)

  def testGetOrderfileName(self):
    """Test method _GetOrderfileName and related methods."""
    ebuild_file = os.path.join(self.tempdir, 'chromeos-chrome-1.1.ebuild')
    ebuild_file_content = '\n'.join([
        'Some message before', 'AFDO_FILE["benchmark"]='
        '"chromeos-chrome-amd64-77.0.3849.0_rc-r1.afdo"',
        'AFDO_FILE["silvermont"]="R77-3809.38-1562580965.afdo"',
        'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)
    self.PatchObject(
        toolchain_util, '_FindEbuildPath', return_value=ebuild_file)
    result = toolchain_util._GetOrderfileName(buildroot=None)
    cwp_name = 'field-77-3809.38-1562580965'
    benchmark_name = 'benchmark-77.0.3849.0-r1'
    self.assertEqual(
        result, 'chromeos-chrome-orderfile-%s-%s' % (cwp_name, benchmark_name))

  def testCompressAFDOFiles(self):
    """Test _CompressAFDOFiles()."""
    input_dir = '/path/to/inputs'
    output_dir = '/another/path/to/outputs'
    targets = ['input1', '/path/to/inputs/input2']
    suffix = '.xz'
    self.PatchObject(cros_build_lib, 'CompressFile')
    # Should raise exception because the input doesn't exist
    with self.assertRaises(RuntimeError) as context:
      toolchain_util._CompressAFDOFiles(targets, input_dir, output_dir, suffix)
    self.assertEqual(
        str(context.exception),
        'file %s to compress does not exist' % os.path.join(
            input_dir, targets[0]))
    # Should pass
    self.PatchObject(os.path, 'exists', return_value=True)
    toolchain_util._CompressAFDOFiles(targets, input_dir, output_dir, suffix)
    compressed_names = [os.path.basename(x) for x in targets]
    inputs = [os.path.join(input_dir, n) for n in compressed_names]
    outputs = [os.path.join(output_dir, n + suffix) for n in compressed_names]
    calls = [mock.call(n, o) for n, o in zip(inputs, outputs)]
    cros_build_lib.CompressFile.assert_has_calls(calls)

  def testGetProfileAge(self):
    """Test top-level function _GetProfileAge()."""
    # Test unsupported artifact_type
    current_day_profile = 'R0-0.0-%d' % int(time.time())
    with self.assertRaises(ValueError) as context:
      toolchain_util._GetProfileAge(current_day_profile, 'unsupported_type')
    self.assertEqual('Only kernel afdo is supported to check profile age.',
                     str(context.exception))

    # Test using profile of the current day.
    ret = toolchain_util._GetProfileAge(current_day_profile, 'kernel_afdo')
    self.assertEqual(0, ret)

    # Test using profile from the last day.
    last_day_profile = 'R0-0.0-%d' % int(time.time() - 86400)
    ret = toolchain_util._GetProfileAge(last_day_profile, 'kernel_afdo')
    self.assertEqual(1, ret)


class FindEbuildPathTest(cros_test_lib.MockTempDirTestCase):
  """Test top-level function _FindEbuildPath()."""

  def setUp(self):
    self.board = 'lulu'
    self.chrome_package = 'chromeos-chrome'
    self.kernel_package = 'chromeos-kernel-3_14'
    self.chrome_ebuild = \
        '/mnt/host/source/src/path/to/chromeos-chrome-1.0.ebuild'
    mock_result = cros_build_lib.CommandResult(output=self.chrome_ebuild)
    self.mock_command = self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=mock_result)

  # pylint: disable=protected-access
  def testInvalidPackage(self):
    """Test invalid package name."""
    with self.assertRaises(ValueError) as context:
      toolchain_util._FindEbuildPath('some-invalid-package')
    self.assertIn('Invalid package name', str(context.exception))
    self.mock_command.assert_not_called()

  def testChromePackagePass(self):
    """Test finding chrome ebuild work."""
    ebuild_file = toolchain_util._FindEbuildPath(self.chrome_package)
    cmd = ['equery', 'w', self.chrome_package]
    self.mock_command.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, self.chrome_ebuild)

  def testKernelPackagePass(self):
    """Test finding kernel ebuild work."""
    ebuild_path = \
        '/mnt/host/source/src/path/to/chromeos-kernel-3_14-3.14-r1.ebuild'
    mock_result = cros_build_lib.CommandResult(output=ebuild_path)
    mock_command = self.PatchObject(
        cros_build_lib, 'RunCommand', return_value=mock_result)
    ebuild_file = toolchain_util._FindEbuildPath(self.kernel_package)
    cmd = ['equery', 'w', self.kernel_package]
    mock_command.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, ebuild_path)

  def testPassWithBoardName(self):
    """Test working with a board name."""
    ebuild_file = toolchain_util._FindEbuildPath(
        self.chrome_package, board='board')
    cmd = ['equery-board', 'w', self.chrome_package]
    self.mock_command.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, self.chrome_ebuild)

  def testReturnPathOutsideChroot(self):
    """Test returning correct path outside chroot."""
    ebuild_file = toolchain_util._FindEbuildPath(
        self.chrome_package, buildroot='/path/to/buildroot')
    self.assertEqual(
        ebuild_file,
        '/path/to/buildroot/src/path/to/chromeos-chrome-1.0.ebuild')


class FindLatestAFDOArtifactTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test _FindLatestAFDOArtifact()."""

  # pylint: disable=protected-access
  def setUp(self):
    self.board = 'board'
    self.unvetted_gs_url = 'gs://path/to/unvetted'
    self.vetted_gs_url = 'gs://path/to/vetted'
    MockListResult = collections.namedtuple('MockListResult',
                                            ('url', 'creation_time'))
    self.unvetted_files = [
        MockListResult(
            url=self.unvetted_gs_url +
            '/chrome-orderfile-77.0.2.0.orderfile.xz',
            creation_time=2.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-77.0.3.0.afdo.xz',
            creation_time=3.0),
        MockListResult(
            url=self.unvetted_gs_url +
            '/chrome-orderfile-77.0.1.0.orderfile.xz',
            creation_time=1.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-afdo-77.0.4.0.afdo.bz2',
            creation_time=4.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-afdo-78.0.2.5.afdo.xz',
            creation_time=5.0),
    ]  # Intentionally unsorted
    self.PatchObject(gs.GSContext, 'List', return_value=self.unvetted_files)
    self.PatchObject(
        toolchain_util, '_FindCurrentChromeBranch', return_value='77')

  def testFindCurrentChromeBranch(self):
    """Test _FindCurrentChromeBranch() works correctly."""
    chrome_name = 'chromeos-chrome-77.0.3845.0_rc-r1.ebuild'
    self.PatchObject(
        toolchain_util,
        '_FindEbuildPath',
        return_value=os.path.join('/path/to', chrome_name))
    branch = '77'
    ret = toolchain_util._FindCurrentChromeBranch()
    self.assertEqual(ret, branch)

  def testFindLatestChromeOrderfile(self):
    """Test returns latest file matching orderfile."""
    orderfile = toolchain_util._FindLatestAFDOArtifact(
        self.unvetted_gs_url, toolchain_util.ORDERFILE_LS_PATTERN)
    self.assertEqual(orderfile, 'chrome-orderfile-77.0.2.0.orderfile.xz')

  def testFindLatestAFDOWithBranch(self):
    """Test returns latest file matching afdo, and also the branch."""
    afdo_on_branch = toolchain_util._FindLatestAFDOArtifact(
        self.unvetted_gs_url, '.afdo')
    self.assertEqual(afdo_on_branch, 'chrome-afdo-77.0.4.0.afdo.bz2')

  def testFindLatestAFDOOnLastBranch(self):
    """Test returns latest file on last branch when current has none."""
    self.PatchObject(
        toolchain_util, '_FindCurrentChromeBranch', return_value='79')
    afdo_on_branch = toolchain_util._FindLatestAFDOArtifact(
        self.unvetted_gs_url, '.afdo')
    self.assertEqual(afdo_on_branch, 'chrome-afdo-78.0.2.5.afdo.xz')


class UploadAFDOArtifactToGSBucketTest(cros_test_lib.MockTempDirTestCase):
  """Test top-level function _UploadAFDOArtifactToGSBucket."""

  # pylint: disable=protected-access
  def setUp(self):
    self.gs_url = 'gs://some/random/gs/url'
    self.local_path = '/path/to/file'
    self.rename = 'new_file_name'
    self.url_without_renaming = os.path.join(self.gs_url, 'file')
    self.url_with_renaming = os.path.join(self.gs_url, 'new_file_name')
    self.mock_copy = self.PatchObject(gs.GSContext, 'Copy')

  def testFileToUploadNotExistTriggerException(self):
    """Test the file to upload doesn't exist in the local path."""
    with self.assertRaises(RuntimeError) as context:
      toolchain_util._UploadAFDOArtifactToGSBucket(self.gs_url, self.local_path)
    self.assertIn('to upload does not exist', str(context.exception))

  def testFileToUploadAlreadyInBucketSkipsException(self):
    """Test uploading a file that already exists in the bucket."""
    self.PatchObject(os.path, 'exists', return_value=True)
    mock_exist = self.PatchObject(gs.GSContext, 'Exists', return_value=True)
    toolchain_util._UploadAFDOArtifactToGSBucket(self.gs_url, self.local_path)
    mock_exist.assert_called_once_with(self.url_without_renaming)
    self.mock_copy.assert_not_called()

  def testFileUploadSuccessWithoutRenaming(self):
    """Test successfully upload a file without renaming."""
    self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(gs.GSContext, 'Exists', return_value=False)
    toolchain_util._UploadAFDOArtifactToGSBucket(self.gs_url, self.local_path)
    self.mock_copy.assert_called_once_with(
        self.local_path, self.url_without_renaming, acl='public-read')

  def testFileUploadSuccessWithRenaming(self):
    """Test successfully upload a file with renaming."""
    self.PatchObject(os.path, 'exists', return_value=True)
    self.PatchObject(gs.GSContext, 'Exists', return_value=False)
    toolchain_util._UploadAFDOArtifactToGSBucket(self.gs_url, self.local_path,
                                                 self.rename)
    self.mock_copy.assert_called_once_with(
        self.local_path, self.url_with_renaming, acl='public-read')


class GenerateChromeOrderfileTest(cros_test_lib.MockTempDirTestCase):
  """Test GenerateChromeOrderfile class."""

  # pylint: disable=protected-access
  def setUp(self):
    self.board = 'board'
    self.out_dir = os.path.join(self.tempdir, 'outdir')
    osutils.SafeMakedirs(self.out_dir)
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    self.working_dir = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(self.working_dir)
    self.working_dir_inchroot = '/tmp'
    self.chroot_args = []
    self.orderfile_name = 'chromeos-chrome-orderfile-1.0'
    self.PatchObject(
        toolchain_util, '_GetOrderfileName', return_value=self.orderfile_name)
    self.test_obj = toolchain_util.GenerateChromeOrderfile(
        self.board, self.out_dir, self.chroot_dir, self.chroot_args)

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
    output = os.path.join(self.working_dir, self.orderfile_name + '.nm')
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
                             self.orderfile_name + '.nm')
    input_orderfile = self.test_obj.INPUT_ORDERFILE_PATH.replace(
        '${BOARD}', self.board)
    output = os.path.join(self.working_dir_inchroot,
                          self.orderfile_name + '.orderfile')
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._PostProcessOrderfile(chrome_nm)
    cmd = [
        self.test_obj.PROCESS_SCRIPT, '--chrome', chrome_nm, '--input',
        input_orderfile, '--output', output
    ]
    cros_build_lib.RunCommand.assert_called_with(
        cmd, enter_chroot=True, chroot_args=self.chroot_args)

  def testSuccessRun(self):
    """Test the main function is running successfully."""
    # Patch the two functions that generate artifacts from inputs that are
    # non-existent without actually building Chrome
    chrome_nm = os.path.join(self.working_dir, self.orderfile_name + '.nm')
    with open(chrome_nm, 'w') as f:
      print('Write something in the nm file', file=f)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_GenerateChromeNM',
        return_value=chrome_nm)
    chrome_orderfile = os.path.join(self.working_dir,
                                    self.orderfile_name + '.orderfile')
    with open(chrome_orderfile, 'w') as f:
      print('Write something in the orderfile', file=f)
    self.PatchObject(
        toolchain_util.GenerateChromeOrderfile,
        '_PostProcessOrderfile',
        return_value=chrome_orderfile)

    self.PatchObject(toolchain_util.GenerateChromeOrderfile, '_CheckArguments')
    mock_upload = self.PatchObject(toolchain_util,
                                   '_UploadAFDOArtifactToGSBucket')

    self.test_obj.Perform()

    # Make sure the tarballs are inside the output directory
    output_files = os.listdir(self.out_dir)
    self.assertIn(
        self.orderfile_name + '.nm' +
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX, output_files)
    self.assertIn(
        self.orderfile_name + '.orderfile' +
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX, output_files)
    self.assertEqual(mock_upload.call_count, 2)


class UpdateEbuildWithAFDOArtifactsTest(cros_test_lib.MockTempDirTestCase):
  """Test UpdateEbuildWithAFDOArtifacts class."""

  # pylint: disable=protected-access
  def setUp(self):
    self.board = 'board'
    self.package = 'valid-package'
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.variable_name = 'VARIABLE_NAME'
    self.variable_value = 'new-afdo-artifact-1.1'
    self.test_obj = toolchain_util.UpdateEbuildWithAFDOArtifacts(
        self.board, self.package, {self.variable_name: self.variable_value})

  def testPatchEbuildFailWithoutMarkers(self):
    """Test _PatchEbuild() fail if the ebuild has no valid markers."""
    ebuild_file = os.path.join(self.tempdir, self.package + '.ebuild')
    osutils.Touch(ebuild_file)
    with self.assertRaises(
        toolchain_util.UpdateEbuildWithAFDOArtifactsError) as context:
      self.test_obj._PatchEbuild(ebuild_file)

    self.assertEqual(
        'Ebuild file does not have appropriate marker for AFDO/orderfile.',
        str(context.exception))

  def testPatchEbuildWithOneRule(self):
    """Test _PatchEbuild() works with only one rule to replace."""
    ebuild_file = os.path.join(self.tempdir, self.package + '.ebuild')
    ebuild_file_content = '\n'.join([
        'Some message before',
        '%s="old-afdo-artifact-1.0"' % self.variable_name, 'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)

    self.test_obj._PatchEbuild(ebuild_file)

    # Make sure temporary file is removed
    self.assertNotIn(self.package + '.ebuild.new', os.listdir(self.tempdir))

    # Make sure the artifact is updated
    pattern = re.compile(
        toolchain_util.AFDO_ARTIFACT_EBUILD_REGEX % self.variable_name)
    found = False
    with open(ebuild_file) as f:
      for line in f:
        matched = pattern.match(line)
        if matched:
          found = True
          self.assertEqual(matched.group('name'), self.variable_value)

    self.assertTrue(found)

  def testPatchEbuildWithMultipleRulesPass(self):
    """Test _PatchEbuild() works with multiple rules to replace."""
    ebuild_file = os.path.join(self.tempdir, self.package + '.ebuild')
    another_variable_name = 'VARIABLE_NAME2'
    another_variable_value = 'another-new-afdo-artifact-2.0'
    ebuild_file_content = '\n'.join([
        'Some message before',
        '%s="old-afdo-artifact-1.0"' % self.variable_name,
        '%s="another-old-afdo-artifact-1.0"' % another_variable_name,
        'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)

    test_obj = toolchain_util.UpdateEbuildWithAFDOArtifacts(
        self.board, self.package, {
            self.variable_name: self.variable_value,
            another_variable_name: another_variable_value
        })

    test_obj._PatchEbuild(ebuild_file)

    # Make sure all patterns are updated.
    patterns = [
        re.compile(
            toolchain_util.AFDO_ARTIFACT_EBUILD_REGEX % self.variable_name),
        re.compile(
            toolchain_util.AFDO_ARTIFACT_EBUILD_REGEX % another_variable_name)
    ]
    values = [self.variable_value, another_variable_value]

    found = 0
    with open(ebuild_file) as f:
      for line in f:
        for p in patterns:
          matched = p.match(line)
          if matched:
            found += 1
            self.assertEqual(matched.group('name'), values[patterns.index(p)])
            break

    self.assertEqual(found, len(patterns))

  def testPatchEbuildWithMultipleRulesFail(self):
    """Test _PatchEbuild() fails when one marker not found in rules."""
    ebuild_file = os.path.join(self.tempdir, self.package + '.ebuild')
    ebuild_file_content = '\n'.join([
        'Some message before',
        '%s="old-afdo-artifact-1.0"' % self.variable_name, 'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)

    test_obj = toolchain_util.UpdateEbuildWithAFDOArtifacts(
        self.board, self.package, {
            self.variable_name: self.variable_value,
            'another_variable_name': 'another_variable_value'
        })

    with self.assertRaises(
        toolchain_util.UpdateEbuildWithAFDOArtifactsError) as context:
      test_obj._PatchEbuild(ebuild_file)
    self.assertEqual(
        'Ebuild file does not have appropriate marker for AFDO/orderfile.',
        str(context.exception))

  def testUpdateManifest(self):
    """Test _UpdateManifest() works properly."""
    ebuild_file = os.path.join(self.tempdir, self.package + '.ebuild')
    cmd = ['ebuild-%s' % self.board, ebuild_file, 'manifest', '--force']
    self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._UpdateManifest(ebuild_file)
    cros_build_lib.RunCommand.assert_called_with(cmd, enter_chroot=True)


class CheckAFDOArtifactExistsTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test CheckAFDOArtifactExists command."""

  def setUp(self):
    self.orderfile_name = 'any_orderfile_name'
    self.afdo_name = 'any_name.afdo'
    self.PatchObject(
        toolchain_util, '_FindCurrentChromeBranch', return_value='78')

  def _CheckExistCall(self, target, url_to_check, board='board'):
    """Helper function to check the Exists() call on a url."""
    for exists in [False, True]:
      mock_exist = self.PatchObject(gs.GSContext, 'Exists', return_value=exists)
      ret = toolchain_util.CheckAFDOArtifactExists(
          buildroot='buildroot', target=target, board=board)
      self.assertEqual(exists, ret)
      mock_exist.assert_called_once_with(url_to_check)

  def testOrderfileGenerateAsTarget(self):
    """Test check orderfile for generation work properly."""
    self.PatchObject(
        toolchain_util, '_GetOrderfileName', return_value=self.orderfile_name)
    self._CheckExistCall(
        'orderfile_generate',
        os.path.join(
            toolchain_util.ORDERFILE_GS_URL_UNVETTED,
            self.orderfile_name + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX))

  def testOrderfileVerifyAsTarget(self):
    """Test check orderfile for verification work properly."""
    self.PatchObject(
        toolchain_util,
        '_FindLatestAFDOArtifact',
        return_value=self.orderfile_name)
    self._CheckExistCall(
        'orderfile_verify',
        os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED,
                     self.orderfile_name))

  def testBenchmarkAFDOAsTarget(self):
    """Test check benchmark AFDO generation work properly."""
    self.PatchObject(
        toolchain_util, '_GetBenchmarkAFDOName', return_value=self.afdo_name)
    self._CheckExistCall(
        'benchmark_afdo',
        os.path.join(toolchain_util.BENCHMARK_AFDO_GS_URL,
                     self.afdo_name + toolchain_util.AFDO_COMPRESSION_SUFFIX))

  def testKernelAFDOAsTarget(self):
    """Test check kernel AFDO verification work properly."""
    self.PatchObject(
        toolchain_util, '_FindLatestAFDOArtifact', return_value=self.afdo_name)
    self._CheckExistCall(
        'kernel_afdo',
        os.path.join(toolchain_util.KERNEL_AFDO_GS_URL_VETTED, '3.14',
                     self.afdo_name), 'lulu')


class AFDOUpdateEbuildTests(cros_test_lib.RunCommandTempDirTestCase):
  """Test wrapper functions to update ebuilds for different types."""

  mock_benchmark_afdo = 'chromeos-chrome-amd64-78.0.3877.0.afdo.bz2'
  mock_cwp_afdo = {
      'silvermont': 'R78-3877.0-1566814872.afdo.xz',
      'airmont': 'R78-3877.0-1566812873.afdo.xz',
      'broadwell': 'R78-3865.35-1566812043.afdo.xz'
  }

  @staticmethod
  def mockFindChromeAFDO(url, _pattern):
    """Mock toolchain_util._FindLatestAFDOArtifact for Chrome AFDO."""
    if 'llvm' in url:
      return AFDOUpdateEbuildTests.mock_benchmark_afdo

    for arch in AFDOUpdateEbuildTests.mock_cwp_afdo:
      if arch in url:
        return AFDOUpdateEbuildTests.mock_cwp_afdo[arch]

  def setUp(self):
    self.board = 'eve'
    self.kver = '4_4'
    self.orderfile = 'chrome.orderfile.xz'
    self.orderfile_stripped = 'chrome.orderfile'
    self.kernel = 'R78-12345.0-1564997810.gcov.xz'
    self.kernel_stripped = 'R78-12345.0-1564997810'
    self.mock_obj = self.PatchObject(
        toolchain_util, 'UpdateEbuildWithAFDOArtifacts', autospec=True)
    self.chrome_branch = '78'
    self.mock_branch = self.PatchObject(
        toolchain_util,
        '_FindCurrentChromeBranch',
        return_value=self.chrome_branch)
    self.mock_warn = self.PatchObject(
        toolchain_util, '_WarnSheriffAboutKernelProfileExpiration')
    self.PatchObject(
        toolchain_util, '_FindCurrentChromeBranch', return_value='78')

  def testOrderfileUpdateChromePass(self):
    """Test OrderfileUpdateChromeEbuild() calls other functions correctly."""
    mock_find = self.PatchObject(
        toolchain_util, '_FindLatestAFDOArtifact', return_value=self.orderfile)

    toolchain_util.OrderfileUpdateChromeEbuild(self.board)
    mock_find.assert_called_once_with(toolchain_util.ORDERFILE_GS_URL_UNVETTED,
                                      toolchain_util.ORDERFILE_LS_PATTERN)
    self.mock_obj.assert_called_with(
        board=self.board,
        package='chromeos-chrome',
        update_rules={'UNVETTED_ORDERFILE': self.orderfile_stripped})

  def testAFDOUpdateChromeEbuildPass(self):
    """Test AFDOUpdateChromeEbuild() calls other functions correctly."""
    mock_find = self.PatchObject(
        toolchain_util,
        '_FindLatestAFDOArtifact',
        side_effect=self.mockFindChromeAFDO)

    ret = toolchain_util.AFDOUpdateChromeEbuild(self.board)
    self.assertTrue(ret)

    calls = [
        mock.call(toolchain_util.BENCHMARK_AFDO_GS_URL,
                  toolchain_util.BENCHMARK_AFDO_LS_PATTERN)
    ]
    update_rules = {
        toolchain_util.AFDO_VARIABLE_REGEX % 'benchmark':
            os.path.splitext(self.mock_benchmark_afdo)[0]
    }
    for arch in toolchain_util.CWP_PROFILE_SUPPORTED_ARCHS:
      url = os.path.join(toolchain_util.CWP_AFDO_GS_URL, arch)
      calls.append(mock.call(url, toolchain_util.CWP_AFDO_LS_PATTERN))
      variable = toolchain_util.AFDO_VARIABLE_REGEX % arch
      update_rules[variable] = os.path.splitext(self.mock_cwp_afdo[arch])[0]

    mock_find.assert_has_calls(calls)
    self.mock_obj.assert_called_with(
        board=self.board, package='chromeos-chrome', update_rules=update_rules)

  def testAFDOUpdateKernelEbuildPass(self):
    """Test AFDOUpdateKernelEbuild() calls other functions correctly."""
    mock_age = self.PatchObject(
        toolchain_util, '_GetProfileAge', return_value=0)
    mock_find = self.PatchObject(
        toolchain_util, '_FindLatestAFDOArtifact', return_value=self.kernel)

    ret = toolchain_util.AFDOUpdateKernelEbuild(self.board)

    self.assertTrue(ret)

    url = os.path.join(toolchain_util.KERNEL_PROFILE_URL,
                       self.kver.replace('_', '.'))
    mock_find.assert_called_once_with(url,
                                      toolchain_util.KERNEL_PROFILE_LS_PATTERN)
    mock_age.assert_called_once_with(self.kernel_stripped, 'kernel_afdo')

    self.mock_warn.assert_not_called()

    self.mock_obj.assert_called_with(
        board=self.board,
        package='chromeos-kernel-' + self.kver,
        update_rules={'AFDO_PROFILE_VERSION': self.kernel_stripped})

  def testAFDOUpdateKernelEbuildFailDueToExpire(self):
    """Test AFDOUpdateKernelEbuild() fails when the profile expires."""
    self.PatchObject(
        toolchain_util,
        '_GetProfileAge',
        return_value=toolchain_util.KERNEL_ALLOWED_STALE_DAYS + 1)
    self.PatchObject(
        toolchain_util, '_FindLatestAFDOArtifact', return_value=self.kernel)

    ret = toolchain_util.AFDOUpdateKernelEbuild(self.board)

    self.assertFalse(ret)

  def testAFDOUpdateKernelEbuildWarnSheriff(self):
    """Test AFDOUpdateKernelEbuild() warns sheriff when profile near expire."""
    self.PatchObject(
        toolchain_util,
        '_GetProfileAge',
        return_value=toolchain_util.KERNEL_ALLOWED_STALE_DAYS - 1)
    self.PatchObject(
        toolchain_util, '_FindLatestAFDOArtifact', return_value=self.kernel)
    ret = toolchain_util.AFDOUpdateKernelEbuild(self.board)
    self.assertTrue(ret)
    self.mock_warn.assert_called_once_with(self.kver, self.kernel_stripped)


class GenerateBenchmarkAFDOProfile(cros_test_lib.MockTempDirTestCase):
  """Test GenerateBenchmarkAFDOProfile class."""

  # pylint: disable=protected-access
  def setUp(self):
    self.buildroot = self.tempdir
    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    osutils.SafeMakedirs(self.chroot_dir)
    self.chroot_args = []
    self.working_dir = os.path.join(self.chroot_dir, 'tmp')
    osutils.SafeMakedirs(self.working_dir)
    self.output_dir = os.path.join(self.tempdir, 'outdir')
    unused = {
        'pv': None,
        'rev': None,
        'category': None,
        'cpv': None,
        'cp': None,
        'cpf': None
    }
    self.package = 'chromeos-chrome'
    self.version = '77.0.3863.0_rc-r1'
    self.chrome_cpv = portage_util.CPV(
        version_no_rev=self.version.split('_')[0],
        package=self.package,
        version=self.version,
        **unused)
    self.board = 'board'
    self.arch = 'amd64'
    self.PatchObject(
        portage_util, 'PortageqBestVisible', return_value=self.chrome_cpv)
    self.PatchObject(portage_util, 'PortageqEnvvar')
    self.test_obj = toolchain_util.GenerateBenchmarkAFDOProfile(
        board=self.board,
        output_dir=self.output_dir,
        chroot_path=self.chroot_dir,
        chroot_args=self.chroot_args)
    self.test_obj.arch = self.arch

  def testDecompressAFDOFile(self):
    """Test _DecompressAFDOFile method."""
    perf_data = 'perf.data.bz2'
    to_decompress = os.path.join(self.working_dir, perf_data)
    mock_uncompress = self.PatchObject(cros_build_lib, 'UncompressFile')
    ret = self.test_obj._DecompressAFDOFile(to_decompress)
    dest = os.path.join(self.working_dir, 'perf.data')
    mock_uncompress.assert_called_once_with(to_decompress, dest)
    self.assertEqual(ret, dest)

  def testGetPerfAFDOName(self):
    """Test _GetPerfAFDOName method."""
    ret = self.test_obj._GetPerfAFDOName()
    perf_data_name = toolchain_util.CHROME_PERF_AFDO_FILE % {
        'package': self.package,
        'arch': self.arch,
        'version': self.version.split('_')[0]
    }
    self.assertEqual(ret, perf_data_name)

  def testCheckAFDOPerfDataStatus(self):
    """Test _CheckAFDOPerfDataStatus method."""
    afdo_name = 'chromeos.afdo'
    url = os.path.join(toolchain_util.BENCHMARK_AFDO_GS_URL,
                       afdo_name + toolchain_util.AFDO_COMPRESSION_SUFFIX)
    for exist in [True, False]:
      mock_exist = self.PatchObject(gs.GSContext, 'Exists', return_value=exist)
      self.PatchObject(
          toolchain_util.GenerateBenchmarkAFDOProfile,
          '_GetPerfAFDOName',
          return_value=afdo_name)
      ret_value = self.test_obj._CheckAFDOPerfDataStatus()
      self.assertEqual(exist, ret_value)
      mock_exist.assert_called_once_with(url)

  def testWaitForAFDOPerfDataTimeOut(self):
    """Test _WaitForAFDOPerfData method with timeout."""

    def mock_timeout(*_args, **_kwargs):
      raise timeout_util.TimeoutError

    self.PatchObject(timeout_util, 'WaitForReturnTrue', new=mock_timeout)
    ret = self.test_obj._WaitForAFDOPerfData()
    self.assertFalse(ret)

  def testWaitForAFDOPerfDataSuccess(self):
    """Test method _WaitForAFDOPerfData() passes."""
    mock_wait = self.PatchObject(timeout_util, 'WaitForReturnTrue')
    afdo_name = 'perf.data'
    mock_get = self.PatchObject(
        toolchain_util.GenerateBenchmarkAFDOProfile,
        '_GetPerfAFDOName',
        return_value=afdo_name)
    mock_check = self.PatchObject(toolchain_util.GenerateBenchmarkAFDOProfile,
                                  '_CheckAFDOPerfDataStatus')
    mock_decompress = self.PatchObject(
        toolchain_util.GenerateBenchmarkAFDOProfile, '_DecompressAFDOFile')
    mock_copy = self.PatchObject(gs.GSContext, 'Copy')
    self.test_obj._WaitForAFDOPerfData()
    mock_wait.assert_called_once_with(
        self.test_obj._CheckAFDOPerfDataStatus,
        timeout=constants.AFDO_GENERATE_TIMEOUT,
        period=constants.SLEEP_TIMEOUT)
    mock_check.assert_called_once()
    # In actual program, this function should be called twice. But since
    # its called _CheckAFDOPerfDataStatus() is mocked, it's only called once
    # in this test.
    mock_get.assert_called_once()
    dest = os.path.join(self.working_dir, 'perf.data.bz2')
    mock_decompress.assert_called_once_with(dest)
    mock_copy.assert_called_once()

  def testCreateAFDOFromPerfData(self):
    """Test method _CreateAFDOFromPerfData()."""
    # Intercept the real path to chrome binary
    mock_chrome_debug = os.path.join(self.working_dir, 'chrome.debug')
    toolchain_util.GenerateBenchmarkAFDOProfile.CHROME_DEBUG_BIN = \
      mock_chrome_debug
    osutils.Touch(mock_chrome_debug)
    perf_name = 'chromeos-chrome-amd64-77.0.3849.0.perf.data'
    self.PatchObject(
        toolchain_util.GenerateBenchmarkAFDOProfile,
        '_GetPerfAFDOName',
        return_value=perf_name)
    afdo_name = 'chromeos-chrome-amd64-77.0.3849.0_rc-r1.afdo'
    self.PatchObject(
        toolchain_util, '_GetBenchmarkAFDOName', return_value=afdo_name)
    mock_command = self.PatchObject(cros_build_lib, 'RunCommand')
    self.test_obj._CreateAFDOFromPerfData()
    afdo_cmd = [
        toolchain_util.GenerateBenchmarkAFDOProfile.AFDO_GENERATE_LLVM_PROF,
        '--binary=/tmp/chrome.unstripped', '--profile=/tmp/' + perf_name,
        '--out=/tmp/' + afdo_name
    ]
    mock_command.assert_called_once_with(
        afdo_cmd,
        enter_chroot=True,
        capture_output=True,
        print_cmd=True,
        chroot_args=self.chroot_args)

  def testUploadArtifacts(self):
    """Test member _UploadArtifacts()."""
    chrome_binary = 'chrome.unstripped'
    afdo_name = 'chrome-1.0.afdo'
    mock_upload = self.PatchObject(toolchain_util,
                                   '_UploadAFDOArtifactToGSBucket')
    self.test_obj._UploadArtifacts(chrome_binary, afdo_name)
    chrome_version = toolchain_util.CHROME_ARCH_VERSION % {
        'package': self.package,
        'arch': self.arch,
        'version': self.version
    }
    upload_name = chrome_version + '.debug' + \
        toolchain_util.AFDO_COMPRESSION_SUFFIX
    calls = [
        mock.call(
            toolchain_util.BENCHMARK_AFDO_GS_URL,
            os.path.join(
                self.output_dir,
                chrome_binary + toolchain_util.AFDO_COMPRESSION_SUFFIX),
            rename=upload_name),
        mock.call(
            toolchain_util.BENCHMARK_AFDO_GS_URL,
            os.path.join(self.output_dir,
                         afdo_name + toolchain_util.AFDO_COMPRESSION_SUFFIX))
    ]
    mock_upload.assert_has_calls(calls)

  def testGenerateAFDOData(self):
    """Test main function of _GenerateAFDOData()."""
    chrome_binary = self.test_obj.CHROME_DEBUG_BIN % {
        'root': self.chroot_dir,
        'board': self.board
    }
    afdo_name = 'chrome.afdo'
    mock_create = self.PatchObject(
        self.test_obj, '_CreateAFDOFromPerfData', return_value=afdo_name)
    mock_compress = self.PatchObject(toolchain_util, '_CompressAFDOFiles')
    mock_upload = self.PatchObject(self.test_obj, '_UploadArtifacts')
    ret = self.test_obj._GenerateAFDOData()
    self.assertEqual(ret, afdo_name)
    mock_create.assert_called_once_with()
    calls = [
        mock.call(
            targets=[chrome_binary],
            input_dir=None,
            output_dir=self.output_dir,
            suffix=toolchain_util.AFDO_COMPRESSION_SUFFIX),
        mock.call(
            targets=[afdo_name],
            input_dir=self.working_dir,
            output_dir=self.output_dir,
            suffix=toolchain_util.AFDO_COMPRESSION_SUFFIX)
    ]
    mock_compress.assert_has_calls(calls)
    mock_upload.assert_called_once_with(chrome_binary, afdo_name)


class UploadVettedAFDOArtifactTest(cros_test_lib.MockTempDirTestCase):
  """Test _UploadVettedAFDOArtifacts()."""

  # pylint: disable=protected-access
  def setUp(self):
    self.artifact = 'some-artifact-1.0'
    self.kver = '3.18'
    self.cwp_arch = 'broadwell'
    self.mock_get = self.PatchObject(
        toolchain_util,
        '_GetArtifactVersionInEbuild',
        return_value=self.artifact)
    self.mock_exist = self.PatchObject(
        gs.GSContext, 'Exists', return_value=False)
    self.mock_upload = self.PatchObject(gs.GSContext, 'Copy')
    self.mock_update_latest = self.PatchObject(toolchain_util,
                                               '_UpdateLatestFileForLegacyPFQ')

  def testWrongArtifactType(self):
    """Test wrong artifact_type raises exception."""
    with self.assertRaises(ValueError) as context:
      toolchain_util._UploadVettedAFDOArtifacts('wrong-type')
    self.assertEqual(
        'Only orderfile, chrome_afdo and kernel_afdo are supported.',
        str(context.exception))
    self.mock_exist.assert_not_called()
    self.mock_upload.assert_not_called()

  def testArtifactExistInGSBucket(self):
    """Test the artifact is already in the GS bucket."""
    mock_exist = self.PatchObject(gs.GSContext, 'Exists', return_value=True)
    ret = toolchain_util._UploadVettedAFDOArtifacts('orderfile')
    mock_exist.assert_called_once()
    self.assertIsNone(ret)

  def testUploadVettedOrderfile(self):
    """Test _UploadVettedAFDOArtifacts() works with orderfile."""
    full_name = self.artifact + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX
    source_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_UNVETTED,
                              full_name)
    dest_url = os.path.join(toolchain_util.ORDERFILE_GS_URL_VETTED, full_name)
    ret = toolchain_util._UploadVettedAFDOArtifacts('orderfile')
    self.mock_get.assert_called_once_with('chromeos-chrome',
                                          'UNVETTED_ORDERFILE')
    self.mock_exist.assert_called_once_with(dest_url)
    self.mock_upload.assert_called_once_with(
        source_url, dest_url, acl='public-read')
    self.assertEqual(ret, self.artifact)

  def testUploadVettedKernelAFDO(self):
    """Test _UploadVettedAFDOArtifacts() works with kernel afdo."""
    full_name = self.artifact + toolchain_util.KERNEL_AFDO_COMPRESSION_SUFFIX
    source_url = os.path.join(toolchain_util.KERNEL_PROFILE_URL, self.kver,
                              full_name)
    dest_url = os.path.join(toolchain_util.KERNEL_AFDO_GS_URL_VETTED, self.kver,
                            full_name)
    ret = toolchain_util._UploadVettedAFDOArtifacts('kernel_afdo', self.kver)
    self.mock_get.assert_called_once_with(
        'chromeos-kernel-' + self.kver.replace('.', '_'),
        'AFDO_PROFILE_VERSION')
    self.mock_exist.assert_called_once_with(dest_url)
    self.mock_upload.assert_called_once_with(
        source_url, dest_url, acl='public-read')
    self.mock_update_latest.assert_not_called()
    self.assertEqual(ret, self.artifact)

  def testUploadVettedChromeBenchmarkAFDO(self):
    """Test _UploadVettedAFDOArtifact works with benchmark afdo."""
    full_name = self.artifact + toolchain_util.AFDO_COMPRESSION_SUFFIX
    source_url = os.path.join(toolchain_util.BENCHMARK_AFDO_GS_URL, full_name)
    dest_url = os.path.join(toolchain_util.BENCHMARK_AFDO_GS_URL_VETTED,
                            full_name)
    ret = toolchain_util._UploadVettedAFDOArtifacts('benchmark_afdo')
    self.mock_get.assert_called_once_with(
        'chromeos-chrome', toolchain_util.AFDO_VARIABLE_REGEX % 'benchmark')
    self.mock_exist.assert_called_once_with(dest_url)
    self.mock_upload.assert_called_once_with(
        source_url, dest_url, acl='public-read')
    self.mock_update_latest.assert_called_once_with(self.artifact)
    self.assertEqual(ret, self.artifact)

  def testUploadVettedChromeCWPAFDO(self):
    """Test _UploadVettedAFDOArtifact works with cwp afdo."""
    full_name = self.artifact + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX
    source_url = os.path.join(toolchain_util.CWP_AFDO_GS_URL, self.cwp_arch,
                              full_name)
    dest_url = os.path.join(toolchain_util.CWP_AFDO_GS_URL_VETTED,
                            self.cwp_arch, full_name)
    ret = toolchain_util._UploadVettedAFDOArtifacts('cwp_afdo', self.cwp_arch)
    self.mock_get.assert_called_once_with(
        'chromeos-chrome', toolchain_util.AFDO_VARIABLE_REGEX % self.cwp_arch)
    self.mock_exist.assert_called_once_with(dest_url)
    self.mock_upload.assert_called_once_with(
        source_url, dest_url, acl='public-read')
    self.mock_update_latest.assert_not_called()
    self.assertEqual(ret, self.artifact)


class PublishVettedAFDOArtifactTest(cros_test_lib.MockTempDirTestCase):
  """Test _PublishVettedAFDOArtifacts()."""

  # pylint: disable=protected-access
  def setUp(self):
    self.package = 'some-package-name-1'
    self.package2 = 'some-package-name-2'
    self.uploaded_exist = {self.package: 'R1234', self.package2: None}
    self.uploaded = {self.package: 'R5678', self.package2: None}
    # Prepare a JSON file containing metadata
    toolchain_util.TOOLCHAIN_UTILS_PATH = self.tempdir
    osutils.SafeMakedirs(os.path.join(self.tempdir, 'afdo_metadata'))
    self.json_file = os.path.join(self.tempdir,
                                  'afdo_metadata/kernel_afdo.json')
    self.afdo_versions = {
        self.package: {
            'name': self.uploaded_exist[self.package],
        },
        self.package2: {
            'name': 'R12345-12',
        },
        'some-package-should-not-change': {
            'name': 'R5678-1234',
        },
    }

    with open(self.json_file, 'w') as f:
      json.dump(self.afdo_versions, f)
    GitStatus = collections.namedtuple('GitStatus', ['output'])
    self.mock_git = self.PatchObject(
        git, 'RunGit', return_value=GitStatus(output='non-empty'))

  def testPublishSameArtifactAsInMetadataFailure(self):
    """Test failure when publishing a same metadata as in JSON file."""
    with self.assertRaises(
        toolchain_util.PublishVettedAFDOArtifactsError) as context:
      toolchain_util._PublishVettedAFDOArtifacts(self.json_file,
                                                 self.uploaded_exist)
    self.assertIn('The artifact to update is the same as in JSON file',
                  str(context.exception))

  def testPublishUploadedProfilesPass(self):
    """Test successfully publish metadata for uploaded profiles."""
    toolchain_util._PublishVettedAFDOArtifacts(self.json_file, self.uploaded)

    # Check changes in JSON file
    new_afdo_versions = json.loads(osutils.ReadFile(self.json_file))
    self.assertEqual(len(self.afdo_versions), len(new_afdo_versions))
    self.assertEqual(new_afdo_versions[self.package]['name'],
                     self.uploaded[self.package])
    for k in self.afdo_versions:
      # Make sure other fields are not changed
      if k != self.package:
        self.assertEqual(self.afdo_versions[k], new_afdo_versions[k])

    # Check the git calls correct
    message = 'afdo_metadata: Publish new profiles.\n\n'
    message += 'Update %s from %s to %s\n' % (self.package, 'R1234', 'R5678')
    calls = [
        mock.call(
            self.tempdir, ['status', '--porcelain', '-uno'],
            capture_output=True,
            print_cmd=True),
        mock.call(self.tempdir, ['diff'], capture_output=True, print_cmd=True),
        mock.call(
            self.tempdir, ['commit', '-a', '-m', message], print_cmd=True),
        mock.call(
            self.tempdir, [
                'push', toolchain_util.TOOLCHAIN_UTILS_REPO,
                'HEAD:refs/for/master%submit'
            ],
            capture_output=True,
            print_cmd=True)
    ]
    self.mock_git.assert_has_calls(calls)


class UploadReleaseChromeAFDOTest(cros_test_lib.MockTempDirTestCase):
  """Test _UploadReleaseChromeAFDO() and related functions."""

  # pylint: disable=protected-access
  def setUp(self):
    self.cwp_name = 'R77-3809.38-1562580965.afdo'
    self.cwp_full = self.cwp_name + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX
    self.arch = 'silvermont'
    self.benchmark_name = 'chromeos-chrome-amd64-77.0.3849.0_rc-r1.afdo'
    self.benchmark_full = \
        self.benchmark_name + toolchain_util.AFDO_COMPRESSION_SUFFIX
    cwp_string = '%s-77-3809.38-1562580965' % self.arch
    benchmark_string = 'benchmark-77.0.3849.0-r1'
    self.merged_name = 'chromeos-chrome-amd64-%s-%s' % (cwp_string,
                                                        benchmark_string)
    self.redacted_name = self.merged_name + '-redacted.afdo'
    self.output = os.path.join(
        self.tempdir,
        self.redacted_name + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX)
    self.decompress = self.PatchObject(cros_build_lib, 'UncompressFile')
    self.compress = self.PatchObject(
        toolchain_util, '_CompressAFDOFiles', return_value=[self.output])
    self.upload = self.PatchObject(toolchain_util,
                                   '_UploadAFDOArtifactToGSBucket')
    self.run_command = self.PatchObject(cros_build_lib, 'RunCommand')
    self.gs_copy = self.PatchObject(gs.GSContext, 'Copy')
    self.PatchObject(osutils.TempDir, '__enter__', return_value=self.tempdir)

  @mock.patch('__builtin__.open')
  def testRedactAFDOProfile(self, mock_open):
    """Test _RedactAFDOProfile() handles calls correctly."""
    input_name = os.path.join(self.tempdir, self.merged_name)
    input_to_text = input_name + '.text.temp'
    redacted_temp = input_name + '.redacted.temp'
    output_name = os.path.join(self.tempdir, self.redacted_name)

    mock_file_obj = io.StringIO()
    mock_open.return_value = mock_file_obj

    toolchain_util._RedactAFDOProfile(input_name, output_name)

    redact_calls = [
        mock.call([
            'llvm-profdata',
            'merge',
            '-sample',
            '-text',
            input_name,
            '-output',
            input_to_text,
        ],
                  enter_chroot=True,
                  print_cmd=True),
        mock.call(['redact_textual_afdo_profile'],
                  input=mock_file_obj,
                  log_stdout_to_file=redacted_temp,
                  enter_chroot=True,
                  print_cmd=True),
        mock.call([
            'llvm-profdata',
            'merge',
            '-sample',
            '-compbinary',
            redacted_temp,
            '-output',
            output_name,
        ],
                  enter_chroot=True,
                  print_cmd=True)
    ]
    self.run_command.assert_has_calls(redact_calls)

  def testUploadReleaseChromeAFDOPass(self):
    """Test _UploadReleaseChromeAFDO() handles naming and calls correctly."""
    redact_call = self.PatchObject(toolchain_util, '_RedactAFDOProfile')

    toolchain_util._UploadReleaseChromeAFDO(self.cwp_name, self.arch,
                                            self.benchmark_name)

    # Check downloading files.
    gs_copy_calls = [
        mock.call(
            os.path.join(toolchain_util.CWP_AFDO_GS_URL, self.arch,
                         self.cwp_full),
            os.path.join(self.tempdir, self.cwp_full)),
        mock.call(
            os.path.join(toolchain_util.BENCHMARK_AFDO_GS_URL,
                         self.benchmark_full),
            os.path.join(self.tempdir, self.benchmark_full))
    ]
    self.gs_copy.assert_has_calls(gs_copy_calls)

    # Check decompress files.
    decompress_calls = [
        mock.call(
            os.path.join(self.tempdir, self.cwp_full),
            os.path.join(self.tempdir, self.cwp_name)),
        mock.call(
            os.path.join(self.tempdir, self.benchmark_full),
            os.path.join(self.tempdir, self.benchmark_name))
    ]
    self.decompress.assert_has_calls(decompress_calls)

    # Check call to merge.
    merge_command = [
        'llvm-profdata',
        'merge',
        '-sample',
        '-output=' + os.path.join(self.tempdir, self.merged_name),
        '-weighted-input=%d,%s' % (toolchain_util.RELEASE_CWP_MERGE_WEIGHT,
                                   os.path.join(self.tempdir, self.cwp_name)),
        '-weighted-input=%d,%s' %
        (toolchain_util.RELEASE_BENCHMARK_MERGE_WEIGHT,
         os.path.join(self.tempdir, self.benchmark_name)),
    ]
    self.run_command.assert_called_once_with(
        merge_command, enter_chroot=True, print_cmd=True)

    # Check calls to redact.
    redact_call.assert_called_once_with(
        os.path.join(self.tempdir, self.merged_name),
        os.path.join(self.tempdir, self.redacted_name))

    # Check compress and upload.
    self.compress.assert_called_once_with(
        [os.path.join(self.tempdir, self.redacted_name)], None, self.tempdir,
        toolchain_util.ORDERFILE_COMPRESSION_SUFFIX)
    self.upload.assert_called_once_with(
        toolchain_util.RELEASE_AFDO_GS_URL_VETTED,
        os.path.join(
            self.tempdir,
            self.redacted_name + toolchain_util.ORDERFILE_COMPRESSION_SUFFIX))


class UploadAndPublishVettedAFDOArtifactsTest(
    cros_test_lib.MockTempDirTestCase):
  """Test UploadAndPublishVettedAFDOArtifacts()."""

  orderfile_name = 'chrome.orderfile'
  kernel_afdo = 'kernel.afdo'
  benchmark_afdo = 'chrome.benchmark.afdo'
  cwp_afdo = 'chrome.cwp.afdo'

  @staticmethod
  def mockUploadVettedAFDOArtifacts(artifact_type, _subcategory=None):
    if artifact_type == 'orderfile':
      return UploadAndPublishVettedAFDOArtifactsTest.orderfile_name
    if artifact_type == 'kernel_afdo':
      return UploadAndPublishVettedAFDOArtifactsTest.kernel_afdo
    if artifact_type == 'benchmark_afdo':
      return UploadAndPublishVettedAFDOArtifactsTest.benchmark_afdo
    if artifact_type == 'cwp_afdo':
      return UploadAndPublishVettedAFDOArtifactsTest.cwp_afdo
    return None

  def setUp(self):
    self.mock_upload = self.PatchObject(
        toolchain_util,
        '_UploadVettedAFDOArtifacts',
        side_effect=self.mockUploadVettedAFDOArtifacts)
    self.mock_publish = self.PatchObject(toolchain_util,
                                         '_PublishVettedAFDOArtifacts')
    self.mock_merge = self.PatchObject(toolchain_util,
                                       '_UploadReleaseChromeAFDO')
    self.board = 'chell'  # Chose chell to test kernel
    self.kver = '3.18'
    self.kernel_json = os.path.join(toolchain_util.TOOLCHAIN_UTILS_PATH,
                                    'afdo_metadata/kernel_afdo.json')
    self.chrome_json = os.path.join(toolchain_util.TOOLCHAIN_UTILS_PATH,
                                    'afdo_metadata/chrome_afdo.json')

  def testReturnFalseWhenNoArtifactUploaded(self):
    """Test it returns False when no new artifacts are uploaded."""
    mock_upload_nothing = self.PatchObject(
        toolchain_util, '_UploadVettedAFDOArtifacts', return_value=None)
    ret = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
        'orderfile', self.board)
    self.assertFalse(ret)
    mock_upload_nothing.assert_called_once_with('orderfile')
    self.mock_publish.assert_not_called()

  def testChromeAFDOPass(self):
    """Make sure for chrome_afdo, it calls other functions correctly."""
    ret = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
        'chrome_afdo', self.board)
    self.assertTrue(ret)
    uploaded = {'benchmark': self.benchmark_afdo}
    upload_calls = [mock.call('benchmark_afdo')]
    merge_calls = []
    for arch in toolchain_util.CWP_PROFILE_SUPPORTED_ARCHS:
      upload_calls.append(mock.call('cwp_afdo', arch))
      uploaded[arch] = self.cwp_afdo
      merge_calls.append(mock.call(self.cwp_afdo, arch, self.benchmark_afdo))
    self.mock_upload.assert_has_calls(upload_calls)
    self.mock_merge.assert_has_calls(merge_calls)
    self.mock_publish.assert_called_once_with(
        self.chrome_json, uploaded,
        'afdo_metadata: Publish new profiles for Chrome.')

  def testKernelAFDOPass(self):
    """Make sure for kernel_afdo, it calls other functions correctly."""
    ret = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
        'kernel_afdo', self.board)
    self.assertTrue(ret)
    uploaded = {
        'chromeos-kernel-' + self.kver.replace('.', '_'): self.kernel_afdo
    }
    self.mock_upload.assert_called_once_with('kernel_afdo', self.kver)
    self.mock_publish.assert_called_once_with(
        self.kernel_json, uploaded,
        'afdo_metadata: Publish new profiles for kernel %s.' % self.kver)

  def testOrderfilePass(self):
    """Make sure for orderfile, it calls other functions correctly."""
    ret = toolchain_util.UploadAndPublishVettedAFDOArtifacts(
        'orderfile', self.board)
    self.assertTrue(ret)
    self.mock_upload.assert_called_once_with('orderfile')
    self.mock_publish.assert_not_called()
