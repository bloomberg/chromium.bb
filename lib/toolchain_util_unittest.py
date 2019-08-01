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

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
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

  def testFindChromeEbuild(self):
    """Test top-level function _FindChromeEbuild."""
    ebuild_path = '/mnt/host/source/src/path/to/chromeos-chrome-1.0.ebuild'
    mock_result = cros_build_lib.CommandResult(output=ebuild_path)
    self.PatchObject(cros_build_lib, 'RunCommand', return_value=mock_result)
    # Test _FindChromeEbuild without inputs
    ebuild_file = toolchain_util._FindChromeEbuild()
    cmd = ['equery', 'w', 'chromeos-chrome']
    cros_build_lib.RunCommand.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, ebuild_path)
    # Test _FindChromeEbuild with a board name
    ebuild_file = toolchain_util._FindChromeEbuild(board='board')
    cmd = ['equery-board', 'w', 'chromeos-chrome']
    cros_build_lib.RunCommand.assert_called_with(
        cmd, enter_chroot=True, redirect_stdout=True)
    self.assertEqual(ebuild_file, ebuild_path)
    # Test _FindChromeEbuild returns path outside chroot
    ebuild_file = toolchain_util._FindChromeEbuild(
        buildroot='/path/to/buildroot')
    self.assertEqual(
        ebuild_file,
        '/path/to/buildroot/src/path/to/chromeos-chrome-1.0.ebuild')

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
        toolchain_util, '_FindChromeEbuild', return_value=ebuild_file)
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
    ebuild_file_content = '\n'.join([
        'Some message before',
        'UNVETTED_ORDERFILE="chromeos-chrome-1.0.orderfile"',
        'Some message after'
    ])
    osutils.WriteFile(ebuild_file, ebuild_file_content)
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


class CheckAFDOArtifactExistsTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test CheckAFDOArtifactExists command."""

  def setUp(self):
    self.orderfile_name = 'any_orderfile_name'
    self.afdo_name = 'any_name.afdo'

  def _CheckExistCall(self, target, url_to_check):
    """Helper function to check the Exists() call on a url."""
    for exists in [False, True]:
      mock_exist = self.PatchObject(gs.GSContext, 'Exists', return_value=exists)
      ret = toolchain_util.CheckAFDOArtifactExists(
          buildroot='buildroot', target=target, board='board')
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
        'FindLatestChromeOrderfile',
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


class OrderfileUpdateChromeEbuildTest(cros_test_lib.RunCommandTempDirTestCase):
  """Test OrderfileUpdateChromeEbuild and related command."""

  # pylint: disable=protected-access
  def setUp(self):
    self.board = 'board'
    self.unvetted_gs_url = 'gs://path/to/unvetted'
    self.vetted_gs_url = 'gs://path/to/vetted'
    MockListResult = collections.namedtuple('MockListResult',
                                            ('url', 'creation_time'))
    self.unvetted_orderfiles = [
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-2.0.orderfile.xz',
            creation_time=2.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-3.0.xz',
            creation_time=3.0),
        MockListResult(
            url=self.unvetted_gs_url + '/chrome-orderfile-1.0.orderfile.xz',
            creation_time=1.0)
    ]  # Intentionally unsorted
    self.PatchObject(toolchain_util, 'UpdateChromeEbuildWithOrderfile')
    self.PatchObject(
        gs.GSContext, 'List', return_value=self.unvetted_orderfiles)

  def testFindLatestChromeOrderfilePass(self):
    """Test FindLatestChromeOrderfile() returns latest orderfile."""
    orderfile = toolchain_util.FindLatestChromeOrderfile(self.unvetted_gs_url)
    self.assertEqual(orderfile, 'chrome-orderfile-2.0.orderfile.xz')

  def testOrderfileUpdateChromePass(self):
    ret = toolchain_util.OrderfileUpdateChromeEbuild(self.board)
    self.assertTrue(ret)
    toolchain_util.UpdateChromeEbuildWithOrderfile.assert_called_with(
        self.board, 'chrome-orderfile-2.0.orderfile.xz')


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
    self.test_obj._GenerateAFDOData()
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
