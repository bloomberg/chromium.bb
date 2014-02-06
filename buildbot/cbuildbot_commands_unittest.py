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
from chromite.lib import gs
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.scripts import pushimage

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


# pylint: disable=W0212
class RunBuildScriptTest(cros_test_lib.TempDirTestCase):
  """Test RunBuildScript in a variety of cases."""

  def _assertRunBuildScript(self, in_chroot=False, error=None, raises=None):
    """Test the RunBuildScript function.

    Args:
      in_chroot: Whether to enter the chroot or not.
      error: error result message to simulate.
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
        with cros_test_lib.LoggingCapturer():
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


class ChromeSDKTest(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Basic tests for ChromeSDK commands with RunCommand mocked out."""
  BOARD = 'daisy_foo'
  EXTRA_ARGS = ('--monkey', 'banana')
  EXTRA_ARGS2 = ('--donkey', 'kong')
  CHROME_SRC = 'chrome_src'
  CMD = ['bar', 'baz']
  CWD = 'fooey'

  def setUp(self):
    self.inst = commands.ChromeSDK(self.CWD, self.BOARD)

  def testRunCommand(self):
    """Test that running a command is possible."""
    self.inst.Run(self.CMD)
    self.assertCommandContains([self.BOARD] + self.CMD, cwd=self.CWD)

  def testRunCommandKwargs(self):
    """Exercise optional arguments."""
    custom_inst = commands.ChromeSDK(
        self.CWD, self.BOARD, extra_args=list(self.EXTRA_ARGS),
        chrome_src=self.CHROME_SRC, debug_log=True)
    custom_inst.Run(self.CMD, list(self.EXTRA_ARGS2))
    self.assertCommandContains(['debug', self.BOARD] + list(self.EXTRA_ARGS) +
                               list(self.EXTRA_ARGS2) + self.CMD, cwd=self.CWD)

  def testNinja(self):
    """Test that running ninja is possible."""
    self.inst.Ninja(self.BOARD)
    self.assertCommandContains([self.BOARD], cwd=self.CWD)

class HWLabCommandsTest(cros_build_lib_unittest.RunCommandTestCase):
  """Test commands related to HWLab tests."""

  def setUp(self):
    self._build = 'test-build'
    self._board = 'test-board'
    self._suite = 'test-suite'
    self._pool = 'test-pool'
    self._num = 42
    self._file_bugs = True
    self._wait_for_results = False
    self._priority = 'test-priority'
    self._timeout_mins = 23

  def testRunHWTestSuiteMinimal(self):
    """Test RunHWTestSuite without optional arguments."""
    commands.RunHWTestSuite(self._build, self._suite, self._board, debug=False)
    self.assertCommandCalled([
        commands._AUTOTEST_RPC_CLIENT, commands._AUTOTEST_RPC_HOSTNAME,
        'RunSuite', '--build', 'test-build', '--suite_name', 'test-suite',
        '--board', 'test-board'
    ], error_code_ok=True)

  def testRunHWTestSuiteMaximal(self):
    """Test RunHWTestSuite with all arguments."""
    commands.RunHWTestSuite(self._build, self._suite, self._board,
                            self._pool, self._num, self._file_bugs,
                            self._wait_for_results, self._priority,
                            self._timeout_mins, debug=False)
    self.assertCommandCalled([
        commands._AUTOTEST_RPC_CLIENT, commands._AUTOTEST_RPC_HOSTNAME,
        'RunSuite', '--build', 'test-build', '--suite_name', 'test-suite',
        '--board', 'test-board', '--pool', 'test-pool', '--num', '42',
        '--file_bugs', 'True', '--no_wait', 'True',
        '--priority', 'test-priority', '--timeout_mins', '23'
    ], error_code_ok=True)

  def testRunHWTestSuiteFailure(self):
    """Test RunHWTestSuite when an error is returned."""
    self.rc.SetDefaultCmdResult(returncode=1)
    self.assertRaises(commands.TestFailure, commands.RunHWTestSuite,
                      self._build, self._suite, self._board, debug=False)


class CBuildBotTest(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Test general cbuildbot command methods."""

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
    tarfile = os.path.join(self.tempdir, 'vm_test_results.tar')
    osutils.Touch(tarfile)
    dump_file_dir, dump_file_name = os.path.split(dump_file)
    ret = [(dump_file_dir, [''], [dump_file_name])]
    with mock.patch('os.walk', return_value=ret):
      gzipped_test_tarball = os.path.join(self.tempdir, 'vm_test_results.tgz')
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


  def testBuild(self, default=False, **kwargs):
    """Base case where Build is called with minimal options."""
    kwargs.setdefault('build_autotest', default)
    kwargs.setdefault('usepkg', default)
    kwargs.setdefault('chrome_binhost_only', default)
    kwargs.setdefault('skip_chroot_upgrade', default)
    commands.Build(buildroot=self._buildroot, board='x86-generic', **kwargs)
    self.assertCommandContains(['./build_packages'])

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_env)."""
    self.testBuild(default=True)

  def testBuildWithEnv(self):
    """Case where Build is called with a custom environment."""
    extra_env = {'A': 'Av', 'B': 'Bv'}
    self.testBuild(extra_env=extra_env)
    self.assertCommandContains(['./build_packages'], extra_env=extra_env)

  def testGenerateSymbols(self):
    """Test GenerateBreakpadSymbols Command."""
    commands.GenerateBreakpadSymbols(self.tempdir, self._board, False)
    self.assertCommandContains(['--board=%s' % self._board])

  @mock.patch('chromite.scripts.upload_symbols.UploadSymbols')
  def testUploadSymbols(self, sym_mock, official=False, cnt=None):
    """Test UploadSymbols Command."""
    sym_mock.side_effect = [0]
    commands.UploadSymbols(self.tempdir, self._board, official, cnt, None)
    self.assertEquals(sym_mock.call_count, 1)
    _, kwargs = sym_mock.call_args
    self.assertEquals(kwargs['official'], official)
    self.assertEquals(kwargs['upload_limit'], cnt)

  def testOfficialUploadSymbols(self):
    """Test uploading symbols for official builds"""
    # Seems pylint can't grasp the @mock.patch decorator.
    # pylint: disable=E1120
    self.testUploadSymbols(official=True)

  def testLimitUploadSymbols(self):
    """Test uploading a limited number of symbols"""
    # Seems pylint can't grasp the @mock.patch decorator.
    # pylint: disable=E1120
    self.testUploadSymbols(cnt=10)

  def testPushImages(self):
    """Test PushImages Command."""
    m = self.PatchObject(pushimage, 'PushImage')
    commands.PushImages(self._board, 'gs://foo/R34-1234.0.0',
                        False, False, None)
    self.assertEqual(m.call_count, 1)

  def testBuildImage(self):
    """Test Basic BuildImage Command."""
    commands.BuildImage(self._buildroot, self._board, None)
    self.assertCommandContains(['./build_image'])

  def testGenerateAuZip(self):
    """Test Basic generate_au_zip Command."""
    with mock.patch.object(git, 'ReinterpretPathForChroot',
                           side_effect=lambda x: x):
      commands.GenerateAuZip(self._buildroot, '/tmp/taco', None)
    self.assertCommandContains(['./build_library/generate_au_zip.py'])

  def testTestAuZip(self):
    """Test Basic generate_au_zip Command."""
    commands.TestAuZip(self._buildroot, '/tmp/taco', None)
    self.assertCommandContains(['./build_library/test_au_zip.py'])

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

  def testAbortCQHWTests(self):
    commands.AbortCQHWTests('my-version', debug=False)
    self.assertCommandContains(['cp', '-'], input='')
    self.assertCommandContains(['-i', 'paladin/my-version'])

  def testHWTestsAborted(self, aborted=True):
    self.PatchObject(gs.GSContext, 'Exists', return_value=aborted)
    self.assertEqual(commands.HaveCQHWTestsBeenAborted('my-version'), aborted)

  def testHWTestsNotAborted(self):
    self.testHWTestsAborted(aborted=False)


class BuildTarballTests(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Tests related to BuildAUTestTarball."""

  def setUp(self):
    self._buildroot = os.path.join(self.tempdir, 'buildroot')
    os.makedirs(self._buildroot)
    self._board = 'test-board'

  def testBuildAUTestTarball(self):
    """Tests that our call to generate an au test tarball is correct."""
    tarball_dir = self.tempdir
    archive_url = 'gs://mytest/path/version'
    with mock.patch.object(commands, 'BuildTarball') as m:
      tarball_path = commands.BuildAUTestTarball(
          self._buildroot, self._board, tarball_dir, 'R26-3928.0.0',
          archive_url)
      m.assert_called_once_with(self._buildroot, ['autotest/au_control_files'],
                                os.path.join(tarball_dir, 'au_control.tar.bz2'),
                                cwd=tarball_dir)

      self.assertEquals(os.path.join(tarball_dir, 'au_control.tar.bz2'),
                        tarball_path)

    # Full release test with partial args defined.
    self.assertCommandContains(['site_utils/autoupdate/full_release_test.py',
                                '--archive_url', archive_url, '3928.0.0',
                                self._board])


class UnmockedTests(cros_test_lib.TempDirTestCase):
  """Test cases which really run tests, instead of using mocks."""

  def testArchiveTestResults(self):
    """Test if we can archive a test results dir."""
    test_results_dir = 'tmp/taco'
    abs_results_dir = os.path.join(self.tempdir, 'chroot', test_results_dir)
    os.makedirs(abs_results_dir)
    osutils.Touch(os.path.join(abs_results_dir, 'foo.txt'))
    res = commands.ArchiveTestResults(self.tempdir, test_results_dir, 'foo.tgz')
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

  def testGenerateHtmlIndexTuple(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('file1', 'monkey tree', 'flying phone',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    for f in files:
      # TODO(build): Use assertIn w/python-2.7.
      self.assertTrue('>%s</a>' % f in html)

  def testGenerateHtmlIndexTupleDupe(self):
    """Verifies GenerateHtmlIndex gives us something unique (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('file1', 'file1', 'file1',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    self.assertEqual(html.count('>file1</a>'), 1)

  def testGenerateHtmlIndexTuplePretty(self):
    """Verifies GenerateHtmlIndex gives us something pretty (input: tuple)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('..|up', 'f.txt|MY FILE', 'm.log|MONKEY', 'b.bin|Yander',)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    osutils.WriteFile('/tmp/foo.html', html)
    for f in files:
      a = f.split('|')
      # TODO(build): Use assertIn w/python-2.7.
      self.assertTrue('href="%s"' % a[0] in html)
      self.assertTrue('>%s</a>' % a[1] in html)

  def testGenerateHtmlIndexDir(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: dir)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('a', 'b b b', 'c', 'dalsdkjfasdlkf',)
    simple_dir = os.path.join(self.tempdir, 'dir')
    for f in files:
      osutils.Touch(os.path.join(simple_dir, f), makedirs=True)
    commands.GenerateHtmlIndex(index, files)
    html = osutils.ReadFile(index)
    for f in files:
      # TODO(build): Use assertIn w/python-2.7.
      self.assertTrue('>%s</a>' % f in html)

  def testGenerateHtmlIndexFile(self):
    """Verifies GenerateHtmlIndex gives us something sane (input: file)"""
    index = os.path.join(self.tempdir, 'index.html')
    files = ('a.tgz', 'b b b.txt', 'c', 'dalsdkjfasdlkf',)
    filelist = os.path.join(self.tempdir, 'listing')
    osutils.WriteFile(filelist, '\n'.join(files))
    commands.GenerateHtmlIndex(index, filelist)
    html = osutils.ReadFile(index)
    for f in files:
      # TODO(build): Use assertIn w/python-2.7.
      self.assertTrue('>%s</a>' % f in html)


if __name__ == '__main__':
  cros_test_lib.main()
