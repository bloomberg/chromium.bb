#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands."""

import os
import sys

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import partial_mock

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


# pylint: disable=W0212
class RunBuildScriptTest(cros_test_lib.TempDirTestCase):

  def _assertRunBuildScript(self, in_chroot=False, error=None, raises=None):
    """Test the RunBuildScript function.

    Args:
      in_chroot: Whether to enter the chroot or not.
      tmpf: If the chroot tempdir exists, a NamedTemporaryFile that contains
            a list of the packages that failed to build.
      raises: If the command should fail, the exception to be raised.
    """
    # Write specified error message to status file.
    def WriteError(_cmd, extra_env=None, **_kwargs):
      if extra_env is not None and error is not None:
        status_file = extra_env['PARALLEL_EMERGE_STATUS_FILE']
        osutils.WriteFile(status_file, error)

    buildroot = self.tempdir
    os.makedirs(os.path.join(buildroot, '.repo'))
    if error is not None:
      os.makedirs(os.path.join(buildroot, 'chroot', 'tmp'))

    # Run the command, throwing an exception if it fails.
    with cros_build_lib_unittest.RunCommandMock() as m:
      cmd = ['example', 'command']
      returncode = 1 if raises else 0
      m.AddCmdResult(cmd, returncode=returncode, side_effect=WriteError)
      with mock.patch.object(git, 'ReinterpretPathForChroot',
                             side_effect=lambda x: x):
        with cros_test_lib.DisableLogging():
          # If the script failed, the exception should be raised and printed.
          if raises:
            self.assertRaises(raises, commands._RunBuildScript, buildroot,
                              cmd, enter_chroot=in_chroot)
          else:
            commands._RunBuildScript(buildroot, cmd, enter_chroot=in_chroot)

  def testSuccessOutsideChroot(self):
    """Test executing a command outside the chroot."""
    self._assertRunBuildScript()

  def testSuccessInsideChrootWithoutTempdir(self):
    """Test executing a command inside a chroot without a tmp dir."""
    self._assertRunBuildScript(in_chroot=True)

  def testSuccessInsideChrootWithTempdir(self):
    """Test executing a command inside a chroot with a tmp dir."""
    self._assertRunBuildScript(in_chroot=True, error='')

  def testFailureOutsideChroot(self):
    """Test a command failure outside the chroot."""
    self._assertRunBuildScript(raises=results_lib.BuildScriptFailure)

  def testFailureInsideChrootWithoutTempdir(self):
    """Test a command failure inside the chroot without a temp directory."""
    self._assertRunBuildScript(in_chroot=True,
                               raises=results_lib.BuildScriptFailure)

  def testFailureInsideChrootWithTempdir(self):
    """Test a command failure inside the chroot with a temp directory."""
    self._assertRunBuildScript(in_chroot=True, error='',
                               raises=results_lib.BuildScriptFailure)

  def testPackageBuildFailure(self):
    """Test detecting a package build failure."""
    self._assertRunBuildScript(in_chroot=True, error=constants.CHROME_CP,
                               raises=results_lib.PackageBuildFailure)


class RunTestSuiteTest(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Test RunTestSuite functionality."""

  TEST_BOARD = 'x86-generic'
  BUILD_ROOT = '/fake/root'

  def _RunTestSuite(self, test_type):
    commands.RunTestSuite(self.tempdir, self.TEST_BOARD, self.BUILD_ROOT,
                          '/tmp/taco', archive_dir='/fake/root',
                          whitelist_chrome_crashes=False,
                          test_type=test_type)

  def testFull(self):
    """Test running FULL config."""
    self._RunTestSuite(constants.FULL_AU_TEST_TYPE)
    self.assertCommandContains(['--quick'], expected=False)
    self.assertCommandContains(['--only_verify'], expected=False)

  def testSimple(self):
    """Test SIMPLE config."""
    self._RunTestSuite(constants.SIMPLE_AU_TEST_TYPE)
    self.assertCommandContains(['--quick'])

  def testSmoke(self):
    """Test SMOKE config."""
    self._RunTestSuite(constants.SMOKE_SUITE_TEST_TYPE)
    self.assertCommandContains(['--quick', '--only_verify'])


class CBuildBotTest(cros_build_lib_unittest.RunCommandTempDirTestCase):

  def setUp(self):
    self._board = 'test-board'
    self._buildroot = self.tempdir
    self._overlays = ['%s/src/third_party/chromiumos-overlay' % self._buildroot]
    self._chroot = os.path.join(self._buildroot, 'chroot')
    os.makedirs(os.path.join(self._buildroot, '.repo'))

  def testGenerateStackTraces(self):
    """Test if we can generate stack traces for minidumps."""
    os.makedirs(os.path.join(self._chroot, 'tmp'))
    dump_file = os.path.join(self._chroot, 'tmp', 'test.dmp')
    tarfile = os.path.join(self.tempdir, 'test_results.tar')
    osutils.Touch(tarfile)
    dump_file_dir, dump_file_name = os.path.split(dump_file)
    ret = [(dump_file_dir, [''], [dump_file_name])]
    with mock.patch('os.walk', return_value=ret):
      gzipped_test_tarball = os.path.join(self.tempdir, 'test_results.tgz')
      commands.GenerateStackTraces(self._buildroot, self._board,
                                   gzipped_test_tarball, self.tempdir, True)
      self.assertCommandContains([gzipped_test_tarball])
      self.assertCommandContains(['tar', 'xf', tarfile, '*.dmp'])
      self.assertCommandContains(['minidump_stackwalk'])
      self.assertCommandContains(['tar', 'uf', tarfile])
      self.assertFalse(os.path.exists(tarfile))

  def testUprevAllPackages(self):
    """Test if we get None in revisions.pfq indicating Full Builds."""
    commands.UprevPackages(self._buildroot, [self._board], self._overlays)
    self.assertCommandContains(['--boards=%s' % self._board, 'commit'])

  def testUploadPrebuilts(self, builder_type=constants.PFQ_TYPE, private=False,
                          chrome_rev=None):
    """Test UploadPrebuilts with a public location."""
    commands.UploadPrebuilts(builder_type, chrome_rev, private,
                             buildroot=self._buildroot, board=self._board)
    self.assertCommandContains([builder_type, 'gs://chromeos-prebuilt'])

  def testUploadPrivatePrebuilts(self):
    """Test UploadPrebuilts with a private location."""
    self.testUploadPrebuilts(private=True)

  def testChromePrebuilts(self):
    """Test UploadPrebuilts for Chrome prebuilts."""
    self.testUploadPrebuilts(builder_type=constants.CHROME_PFQ_TYPE,
                             chrome_rev='tot')

  def testSdkPrebuilts(self):
    """Test UploadPrebuilts for SDK builds."""
    # A magical date for a magical time.
    version = '1994.04.02.000000'

    # Fake out toolchain tarballs.
    tarball_dir = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                               constants.SDK_TOOLCHAINS_OUTPUT)
    osutils.SafeMakedirs(tarball_dir)

    tarball_args = []
    for tarball_base in ('i686', 'arm-none-eabi'):
      tarball = '%s.tar.xz' % tarball_base
      tarball_path = os.path.join(tarball_dir, tarball)
      osutils.Touch(tarball_path)
      tarball_arg = '%s:%s' % (tarball_base, tarball_path)
      tarball_args.append(['--toolchain-tarball', tarball_arg])

    with mock.patch.object(commands, '_GenerateSdkVersion',
                           return_value=version):
      self.testUploadPrebuilts(builder_type=constants.CHROOT_BUILDER_TYPE)
    self.assertCommandContains(['--toolchain-upload-path',
                                '1994/04/%%(target)s-%(version)s.tar.xz'])
    for args in tarball_args:
      self.assertCommandContains(args)
    self.assertCommandContains(['--set-version', version])
    self.assertCommandContains(['--prepackaged-tarball',
                                os.path.join(self._buildroot,
                                             'built-sdk.tar.xz')])

  def testDevInstallerPrebuilts(self, packages=('package1', 'package2')):
    """Test UploadDevInstallerPrebuilts."""
    args = ['gs://dontcare', 'some_path_to_key', 'https://my_test/location']
    with mock.patch.object(commands, 'AddPackagesForPrebuilt',
                           return_value=packages):
      commands.UploadDevInstallerPrebuilts(*args, buildroot=self._buildroot,
                                           board=self._board)
    self.assertCommandContains([constants.CANARY_TYPE] + args[2:] + args[0:2])

  def testAddPackagesForPrebuilt(self):
    """Test AddPackagesForPrebuilt."""
    self.assertEqual(commands.AddPackagesForPrebuilt('/'), None)

    data = """# comment!
cat/pkg-0
ca-t2/pkg2-123
ca-t3/pk-g4-4.0.1-r333
"""
    pkgs = [
        'cat/pkg',
        'ca-t2/pkg2',
        'ca-t3/pk-g4',
    ]
    cmds = ['--packages=' + x for x in pkgs]
    f = os.path.join(self.tempdir, 'package.provided')
    osutils.WriteFile(f, data)
    self.assertEqual(commands.AddPackagesForPrebuilt(f), cmds)

  def testMissingDevInstallerFile(self):
    """Test that we raise an exception when the installer file is missing."""
    self.assertRaises(commands.PackageFileMissing,
                      self.testDevInstallerPrebuilts, packages=())


  def testBuild(self, default=False, **kwds):
    """Base case where Build is called with minimal options."""
    kwds.setdefault('build_autotest', default)
    kwds.setdefault('usepkg', default)
    kwds.setdefault('skip_toolchain_update', default)
    kwds.setdefault('nowithdebug', default)
    commands.Build(buildroot=self._buildroot, board='x86-generic', **kwds)
    self.assertCommandContains(['./build_packages'])

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_env)."""
    self.testBuild(default=True)

  def testBuildWithEnv(self):
    """Case where Build is called with a custom environment."""
    extra_env = {'A': 'Av', 'B': 'Bv'}
    self.testBuild(extra_env=extra_env)
    self.assertCommandContains(['./build_packages'], extra_env=extra_env)

  def testUploadSymbols(self, official=False):
    """Test UploadSymbols Command."""
    commands.UploadSymbols(self.tempdir, self._board, official=official)
    self.assertCommandContains(['--official_build'], expected=official)

  def testOfficialUploadSymbols(self):
    self.testUploadSymbols(official=True)

  def testPushImages(self, profile=None):
    """Test PushImages Command."""
    commands.PushImages(self._buildroot, self._board, 'branch_name', 'gs://foo',
                        dryrun=False, profile=profile)
    self.assertCommandContains(['./pushimage', '--branch=branch_name'])

  def testPushImagesWithProfile(self):
    """Test PushImages Command with profile."""
    self.testPushImages(profile='some_profile')
    self.assertCommandContains(['--profile=some_profile'])

  def testBuildImage(self):
    """Test Basic BuildImage Command."""
    commands.BuildImage(self._buildroot, self._board, None)
    self.assertCommandContains(['./build_image'])

  def testCompleteBuildImage(self):
    """Test Complete BuildImage Command."""
    images_to_build = ['bob', 'carol', 'ted', 'alice']
    commands.BuildImage(self._buildroot, self._board, images_to_build,
        rootfs_verification=False, extra_env={'LOVE': 'free'},
        disk_layout='2+2', version='1969')
    self.assertCommandContains(['./build_image'])

  def _TestChromeLKGM(self, chrome_revision):
    """Helper method for testing the GetChromeLKGM method.

    Args:
      chrome_revision: either a number or None.
    """
    chrome_lkgm = '3322.0.0'
    output = '\n\n%s\n' % chrome_lkgm
    self.rc.AddCmdResult(partial_mock.In('svn'), output=output)
    self.assertEqual(chrome_lkgm, commands.GetChromeLKGM(chrome_revision))

  def testChromeLKGM(self):
    """Verifies that we can get the chrome lkgm without a chrome revision."""
    self._TestChromeLKGM(None)

  def testChromeLKGMWithRevision(self):
    """Verifies that we can get the chrome lkgm with a chrome revision."""
    self._TestChromeLKGM(1234)
    self.assertCommandContains(['svn', 'cat', '-r', '1234'])


class UnmockedTests(cros_test_lib.TempDirTestCase):

  def testArchiveTestResults(self):
    """Test if we can archive a test results dir."""
    test_results_dir = 'tmp/taco'
    abs_results_dir = os.path.join(self.tempdir, 'chroot', test_results_dir)
    os.makedirs(abs_results_dir)
    osutils.Touch(os.path.join(abs_results_dir, 'foo.txt'))
    res = commands.ArchiveTestResults(self.tempdir, test_results_dir, '')
    cros_test_lib.VerifyTarball(res, ['./', 'foo.txt'])

  def testBuildFirmwareArchive(self):
    """Verifies that firmware archiver includes proper files"""
    # Assorted set of file names, some of which are supposed to be included in
    # the archive.
    fw_files = (
      'dts/emeraldlake2.dts',
      'image-link.rw.bin',
      'nv_image-link.bin',
      'pci8086,0166.rom',
      'seabios.cbfs',
      'u-boot.elf',
      'u-boot_netboot.bin',
      'updater-link.rw.sh',
      'x86-memtest')
    # Files which should be included in the archive.
    fw_archived_files = (
      'dts/emeraldlake2.dts',
      'image-link.rw.bin',
      'nv_image-link.bin',
      'updater-link.rw.sh')
    board = 'link'
    fw_test_root = os.path.join(self.tempdir, os.path.basename(__file__))
    fw_files_root = os.path.join(fw_test_root,
                                 'chroot/build/%s/firmware' % board)
    # Generate a representative set of files produced by a typical build.
    cros_test_lib.CreateOnDiskHierarchy(fw_files_root, fw_files)
    # Create an archive from the simulated firmware directory
    tarball = os.path.join(
        fw_test_root,
        commands.BuildFirmwareArchive(fw_test_root, board, fw_test_root))
    # Verify the tarball contents.
    cros_test_lib.VerifyTarball(tarball, fw_archived_files)

if __name__ == '__main__':
  cros_test_lib.main()
