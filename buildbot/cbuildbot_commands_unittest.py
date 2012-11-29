#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import mox
import os
import shutil
import sys
import tempfile

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gclient
from chromite.lib import git


# pylint: disable=E1101,W0212,R0904
class RunBuildScriptTest(cros_test_lib.MoxTestCase):

  def _assertRunBuildScript(self, in_chroot=False, tmpf=None, raises=None):
    """Test the RunBuildScript function.

    Args:
      in_chroot: Whether to enter the chroot or not.
      tmpf: If the chroot tempdir exists, a NamedTemporaryFile that contains
            a list of the packages that failed to build.
      raises: If the command should fail, the exception to be raised.
    """

    # Mock out functions used by RunBuildScript.
    self.mox.StubOutWithMock(git, 'ReinterpretPathForChroot')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'Error')
    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(tempfile, 'NamedTemporaryFile')

    buildroot = '.'
    cmd = ['example', 'command']

    # If we enter the chroot, _RunBuildScript will try to create a temporary
    # status file inside the chroot to track what packages failed (if any.)
    kwargs = dict()
    if in_chroot:
      tempdir = os.path.join(buildroot, 'chroot', 'tmp')
      os.path.exists(tempdir).AndReturn(tmpf is not None)
      if tmpf is not None:
        tempfile.NamedTemporaryFile(dir=tempdir).AndReturn(tmpf)
        git.ReinterpretPathForChroot(tmpf.name).AndReturn(tmpf.name)
        kwargs['extra_env'] = {'PARALLEL_EMERGE_STATUS_FILE': tmpf.name}

    # Run the command, throwing an exception if it fails.
    ret = cros_build_lib.RunCommand(cmd, cwd=buildroot, enter_chroot=in_chroot,
                                    **kwargs)
    if raises:
      result = cros_build_lib.CommandResult()
      ex = cros_build_lib.RunCommandError('command totally failed', result)
      ret.AndRaise(ex)
      cros_build_lib.Error('\n%s', ex)

    # If the script failed, the exception should be raised and printed.
    self.mox.ReplayAll()
    if raises:
      self.assertRaises(raises, commands._RunBuildScript, buildroot,
                        cmd, enter_chroot=in_chroot)
    else:
      commands._RunBuildScript(buildroot, cmd, enter_chroot=in_chroot)
    self.mox.VerifyAll()

  def testSuccessOutsideChroot(self):
    """Test executing a command outside the chroot."""
    self._assertRunBuildScript()

  def testSuccessInsideChrootWithoutTempdir(self):
    """Test executing a command inside a chroot without a tmp dir."""
    self._assertRunBuildScript(in_chroot=True)

  def testSuccessInsideChrootWithTempdir(self):
    """Test executing a command inside a chroot with a tmp dir."""
    with tempfile.NamedTemporaryFile() as tmpf:
      self._assertRunBuildScript(in_chroot=True, tmpf=tmpf)

  def testFailureOutsideChroot(self):
    """Test a command failure outside the chroot."""
    self._assertRunBuildScript(raises=results_lib.BuildScriptFailure)

  def testFailureInsideChrootWithoutTempdir(self):
    """Test a command failure inside the chroot without a temp directory."""
    self._assertRunBuildScript(in_chroot=True,
                               raises=results_lib.BuildScriptFailure)

  def testFailureInsideChrootWithTempdir(self):
    """Test a command failure inside the chroot with a temp directory."""
    with tempfile.NamedTemporaryFile() as tmpf:
      self._assertRunBuildScript(in_chroot=True, tmpf=tmpf,
                                 raises=results_lib.BuildScriptFailure)

  def testPackageBuildFailure(self):
    """Test detecting a package build failure."""
    with tempfile.NamedTemporaryFile() as tmpf:
      tmpf.write(constants.CHROME_CP)
      self._assertRunBuildScript(in_chroot=True, tmpf=tmpf,
                                 raises=results_lib.PackageBuildFailure)


class RunTestSuiteTest(cros_test_lib.MoxTempDirTestCase):
  """Test RunTestSuite functionality."""

  TEST_BOARD = 'x86-generic'
  BUILD_ROOT = '/fake/root'

  def setUp(self):
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')

  def _RunTestSuite(self, test_type):
    commands.RunTestSuite(self.tempdir, self.TEST_BOARD, self.BUILD_ROOT,
                          '/tmp/taco', archive_dir='/fake/root',
                          whitelist_chrome_crashes=False,
                          test_type=test_type)

  def _MockOutRunCommand(self, cmd):
    """Function to mock out the main RunCommand invocation."""
    cwd = self.tempdir + '/src/scripts'
    obj = cros_test_lib.EasyAttr(returncode=0)

    cros_build_lib.RunCommand(cmd, cwd=cwd,
        error_code_ok=True).AndReturn(obj)

  @staticmethod
  def _NoItemsInList(items, list_):
    """Helper function that returns True if all items are not in a list."""
    return set(items).isdisjoint(set(list_))

  def tearDown(self):
    self.mox.VerifyAll()

  def testFull(self):
    """Test running FULL config."""
    cond = mox.Func(
        lambda x: self._NoItemsInList(['--quick', '--only_verify'], x))
    self._MockOutRunCommand(cond)
    self.mox.ReplayAll()
    self._RunTestSuite(constants.FULL_AU_TEST_TYPE)

  def testSimple(self):
    """Test SIMPLE config."""
    self._MockOutRunCommand(mox.In('--quick'))
    self.mox.ReplayAll()
    self._RunTestSuite(constants.SIMPLE_AU_TEST_TYPE)

  def testSmoke(self):
    """Test SMOKE config."""
    self._MockOutRunCommand(mox.And(mox.In('--quick'), mox.In('--only_verify')))
    self.mox.ReplayAll()
    self._RunTestSuite(constants.SMOKE_SUITE_TEST_TYPE)


class CBuildBotTest(cros_test_lib.MoxTempDirTestCase):

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self._test_repos = [['kernel', 'third_party/kernel/files'],
                        ['login_manager', 'platform/login_manager']
                       ]
    self._test_cros_workon_packages = (
        'chromeos-base/kernel\nchromeos-base/chromeos-login\n')
    self._test_board = 'test-board'
    self._buildroot = '.'
    self._test_dict = {'kernel': ['chromos-base/kernel', 'dev-util/perf'],
                       'cros': ['chromos-base/libcros']
                      }
    self._test_string = 'kernel.git@12345test cros.git@12333test'
    self._test_string += ' crosutils.git@blahblah'
    self._revision_file = 'test-revisions.pfq'
    self._test_parsed_string_array = [['chromeos-base/kernel', '12345test'],
                                      ['dev-util/perf', '12345test'],
                                      ['chromos-base/libcros', '12345test']]
    self._overlays = ['%s/src/third_party/chromiumos-overlay' % self._buildroot]
    self._chroot_overlays = [
        git.ReinterpretPathForChroot(p) for p in self._overlays
    ]
    self._CWD = os.path.dirname(os.path.realpath(__file__))
    os.makedirs(self.tempdir + '/chroot/tmp/taco')

  def testArchiveTestResults(self):
    """Test if we can archive the latest results dir to Google Storage."""
    # Set vars for call.
    self.mox.StubOutWithMock(shutil, 'rmtree')
    buildroot = '/fake_dir'
    test_tarball = os.path.join(buildroot, 'test_results.tgz')
    test_results_dir = 'fake_results_dir'

    # Convenience variables to make archive easier to understand.
    chroot = os.path.join(buildroot, 'chroot')
    path_to_results = os.path.join(chroot, test_results_dir)
    cros_build_lib.SudoRunCommand(
        ['chmod', '-R', 'a+rw', path_to_results], print_cmd=False)
    self.mox.StubOutWithMock(cros_build_lib, 'CreateTarball')
    cros_build_lib.CreateTarball(
        test_tarball, path_to_results, chroot=chroot,
        compression=cros_build_lib.COMP_GZIP)
    shutil.rmtree(path_to_results)
    self.mox.ReplayAll()
    commands.ArchiveTestResults(buildroot, test_results_dir, '')
    self.mox.VerifyAll()

  def testGenerateMinidumpStackTraces(self):
    """Test if we can generate stack traces for minidumps."""
    temp_dir = '/chroot/temp_dir'
    gzipped_test_tarball = '/test_results.tgz'
    test_tarball = '/test_results.tar'
    dump_file = os.path.join(temp_dir, 'test.dmp')
    buildroot = '/'
    board = 'test_board'
    symbol_dir = os.path.join('/build', board, 'usr', 'lib', 'debug',
                              'breakpad')
    cwd = os.path.join(buildroot, 'src', 'scripts')
    archive_dir = '/archive/dir'

    self.mox.StubOutWithMock(tempfile, 'mkdtemp')
    tempfile.mkdtemp(dir=mox.IgnoreArg(), prefix=mox.IgnoreArg()). \
        AndReturn(temp_dir)
    self.mox.StubOutWithMock(os, 'walk')
    dump_file_dir, dump_file_name = os.path.split(dump_file)
    os.walk(mox.IgnoreArg()).AndReturn([(dump_file_dir, [''],
                                       [dump_file_name])])
    self.mox.StubOutWithMock(git, 'ReinterpretPathForChroot')
    git.ReinterpretPathForChroot(mox.IgnoreArg()).AndReturn(dump_file)
    self.mox.StubOutWithMock(commands, 'ArchiveFile')
    self.mox.StubOutWithMock(os, 'unlink')
    self.mox.StubOutWithMock(shutil, 'rmtree')

    gzip = cros_build_lib.FindCompressor(cros_build_lib.COMP_GZIP)
    cros_build_lib.RunCommand([gzip, '-df', gzipped_test_tarball])
    cros_build_lib.RunCommand(
        ['tar',
         'xf',
         test_tarball,
         '--directory=%s' % temp_dir,
         '--wildcards', '*.dmp'],
         error_code_ok=True,
         redirect_stderr=True).AndReturn(cros_build_lib.CommandResult())
    stack_trace = '%s.txt' % dump_file
    cros_build_lib.RunCommand(
        ['minidump_stackwalk', dump_file, symbol_dir], cwd=cwd,
        enter_chroot=True, error_code_ok=True, log_stdout_to_file=stack_trace,
        redirect_stderr=True)
    commands.ArchiveFile(stack_trace, archive_dir)
    cros_build_lib.RunCommand(
        ['tar', 'uf', test_tarball, '--directory=%s' % temp_dir, '.'])
    cros_build_lib.RunCommand(
        '%s -c %s > %s' % (gzip, test_tarball, gzipped_test_tarball),
        shell=True)
    os.unlink(test_tarball)
    shutil.rmtree(temp_dir)

    self.mox.ReplayAll()
    commands.GenerateMinidumpStackTraces(buildroot, board,
                                         gzipped_test_tarball,
                                         archive_dir)
    self.mox.VerifyAll()

  def testUprevAllPackages(self):
    """Test if we get None in revisions.pfq indicating Full Builds."""
    drop_file = commands._PACKAGE_FILE % {'buildroot': self._buildroot}
    path = git.ReinterpretPathForChroot(constants.CHROMITE_BIN_DIR)
    cros_build_lib.RunCommand(
        [os.path.join(path, 'cros_mark_as_stable'), '--all',
         '--boards=%s' % self._test_board,
         '--overlays=%s' % ':'.join(self._chroot_overlays),
         '--drop_file=%s' % git.ReinterpretPathForChroot(drop_file),
         'commit'],
        cwd='.',
        enter_chroot=True)

    self.mox.ReplayAll()
    commands.UprevPackages(self._buildroot,
                           [self._test_board],
                           self._overlays)
    self.mox.VerifyAll()

  def testUploadPublicPrebuilts(self):
    """Test UploadPrebuilts with a public location."""
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-prebuilt'),
                    mox.In(constants.PFQ_TYPE))
    cros_build_lib.RunCommand(check, cwd=constants.CHROMITE_BIN_DIR)
    self.mox.ReplayAll()
    commands.UploadPrebuilts(constants.PFQ_TYPE, None, False,
                             buildroot=self._buildroot,
                             board=self._test_board)
    self.mox.VerifyAll()

  def testUploadPrivatePrebuilts(self):
    """Test UploadPrebuilts with a private location."""
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-prebuilt'),
                    mox.In(constants.PFQ_TYPE))

    cros_build_lib.RunCommand(check, cwd=constants.CHROMITE_BIN_DIR)
    self.mox.ReplayAll()
    commands.UploadPrebuilts(constants.PFQ_TYPE, None, True,
                             buildroot=self._buildroot,
                             board=self._test_board)
    self.mox.VerifyAll()

  def testChromePrebuilts(self):
    """Test UploadPrebuilts for Chrome prebuilts."""
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-prebuilt'),
                    mox.In(constants.CHROME_PFQ_TYPE))
    cros_build_lib.RunCommand(check, cwd=constants.CHROMITE_BIN_DIR)
    self.mox.ReplayAll()
    commands.UploadPrebuilts(constants.CHROME_PFQ_TYPE, 'tot', False,
                             buildroot=self._buildroot,
                             board=self._test_board)
    self.mox.VerifyAll()

  def testDevInstallerPrebuilts(self):
    """Test UploadDevInstallerPrebuilts."""
    test_bucket = 'gs://dontcare'
    test_key = 'some_path_to_key'
    test_url = 'https://my_test/location'
    package_list = ['package1', 'package2']
    self.mox.StubOutWithMock(commands, 'AddPackagesForPrebuilt')
    check = mox.And(mox.IsA(list),
                    mox.In(test_bucket),
                    mox.In(constants.CANARY_TYPE),
                    mox.In(test_key),
                    mox.In(test_url))
    commands.AddPackagesForPrebuilt(mox.StrContains(
        commands._BINHOST_PACKAGE_FILE)).AndReturn(package_list)
    cros_build_lib.RunCommand(check, cwd=constants.CHROMITE_BIN_DIR)
    self.mox.ReplayAll()
    commands.UploadDevInstallerPrebuilts(test_bucket, test_key, test_url,
                                         buildroot=self._buildroot,
                                         board=self._test_board)
    self.mox.VerifyAll()

  def testMissingDevInstallerFile(self):
    """Test that we raise an exception when the installer file is missing."""
    test_bucket = 'gs://dontcare'
    test_key = 'some_path_to_key'
    test_url = 'https://my_test/location'
    package_list = []
    self.mox.StubOutWithMock(commands, 'AddPackagesForPrebuilt')
    commands.AddPackagesForPrebuilt(mox.StrContains(
        commands._BINHOST_PACKAGE_FILE)).AndReturn(package_list)
    self.mox.ReplayAll()
    self.assertRaises(commands.PackageFileMissing,
                      commands.UploadDevInstallerPrebuilts,
                      test_bucket, test_key, test_url,
                      buildroot=self._buildroot, board=self._test_board)
    self.mox.VerifyAll()


  def testBuildMinimal(self):
    """Base case where Build is called with minimal options."""
    buildroot = '/bob/'
    cmd = ['./build_packages', '--nowithautotest',
           '--board=x86-generic'] + commands._LOCAL_BUILD_FLAGS
    cros_build_lib.RunCommand(mox.SameElementsAs(cmd),
                              cwd=mox.StrContains(buildroot),
                              chroot_args=[],
                              enter_chroot=True,
                              extra_env=None)
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   build_autotest=False,
                   usepkg=False,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   )
    self.mox.VerifyAll()

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_evn)."""
    buildroot = '/bob/'
    arg_test = mox.SameElementsAs(['./build_packages',
                                   '--board=x86-generic',
                                   '--skip_toolchain_update',
                                   '--nowithdebug'])
    cros_build_lib.RunCommand(arg_test,
                              cwd=mox.StrContains(buildroot),
                              chroot_args=[],
                              enter_chroot=True,
                              extra_env=None)
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   build_autotest=True,
                   usepkg=True,
                   skip_toolchain_update=True,
                   nowithdebug=True,
                   )
    self.mox.VerifyAll()

  def testBuildWithEnv(self):
    """Case where Build is called with a custom environment."""
    buildroot = '/bob/'
    extra = {'A' :'Av', 'B' : 'Bv'}
    cros_build_lib.RunCommand(
        mox.IgnoreArg(),
        cwd=mox.StrContains(buildroot),
        chroot_args=[],
        enter_chroot=True,
        extra_env=mox.And(
            mox.ContainsKeyValue('A', 'Av'), mox.ContainsKeyValue('B', 'Bv')))
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   build_autotest=False,
                   usepkg=False,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   extra_env=extra)
    self.mox.VerifyAll()

  def testUploadSymbols(self):
    """Test UploadSymbols Command."""
    buildroot = '/bob'
    board = 'board_name'

    cros_build_lib.RunCommandCaptureOutput(
        ['./upload_symbols', '--board=board_name', '--yes', '--verbose',
         '--official_build'], cwd='/bob/src/scripts',
        enter_chroot=True, combine_stdout_stderr=True)

    cros_build_lib.RunCommandCaptureOutput(
        ['./upload_symbols', '--board=board_name', '--yes', '--verbose'],
        cwd='/bob/src/scripts', enter_chroot=True, combine_stdout_stderr=True)

    self.mox.ReplayAll()
    commands.UploadSymbols(buildroot, board, official=True)
    commands.UploadSymbols(buildroot, board, official=False)
    self.mox.VerifyAll()

  def testPushImages(self):
    """Test PushImages Command."""
    buildroot = '/bob'
    board = 'board_name'
    branch_name = 'branch_name'
    archive_url = 'gs://archive/url'

    cros_build_lib.RunCommand(
        ['./pushimage', '--board=board_name', '--branch=branch_name',
         archive_url], cwd=mox.StrContains('crostools'))

    self.mox.ReplayAll()
    commands.PushImages(buildroot, board, branch_name, archive_url, False, None)
    self.mox.VerifyAll()

  def testBuildImage(self):
    """Test Basic BuildImage Command."""
    buildroot = '/bob'
    board = 'board_name'

    cros_build_lib.RunCommand(
        ['./build_image',
         '--board=%s' % board,
         '--replace',
         '--version=',
         'base'],
      cwd=buildroot,
      enter_chroot=True,
      extra_env=None)

    self.mox.ReplayAll()
    commands.BuildImage(buildroot, board, None)
    self.mox.VerifyAll()

  def testCompleteBuildImage(self):
    """Test Complete BuildImage Command."""
    buildroot = '/paul'
    board = 'columbia'
    images_to_build = ['bob', 'carol', 'ted', 'alice']
    version = '1969'
    disk_layout = '2+2'
    extra_env = {'LOVE': 'free'}

    cros_build_lib.RunCommand([
      './build_image',
      '--board=%s' % board,
      '--replace',
      '--version=%s' % version,
      '--noenable_rootfs_verification',
      '--disk_layout=%s' % disk_layout, ]
      + images_to_build,
      cwd=buildroot,
      enter_chroot=True,
      extra_env=extra_env)

    self.mox.ReplayAll()
    commands.BuildImage(buildroot, board, images_to_build,
      rootfs_verification=False, extra_env=extra_env, disk_layout=disk_layout,
      version=version)
    self.mox.VerifyAll()

  def testPushImages2(self):
    """Test PushImages Command with profile."""
    buildroot = '/bob'
    board = 'board_name'
    branch_name = 'branch_name'
    profile_name = 'profile_name'
    archive_url = 'gs://archive/url'

    cros_build_lib.RunCommand(
        ['./pushimage', '--board=board_name', '--profile=profile_name',
         '--branch=branch_name', archive_url], cwd=mox.StrContains('crostools'))

    self.mox.ReplayAll()
    commands.PushImages(buildroot, board, branch_name, archive_url,
                        dryrun=False, profile=profile_name)
    self.mox.VerifyAll()

  def _TestChromeLKGM(self, chrome_revision):
    """Helper method for testing the GetChromeLKGM method.

    Args:
      chrome_revision: either a number or None.
    """
    self.mox.StubOutWithMock(gclient, 'GetBaseURLs')
    svn_url = 'http://dontcare'
    full_svn_url = '%s/trunk/src/chromeos/CHROMEOS_LKGM' % svn_url
    chrome_lkgm_expected = '3322.0.0'
    result = cros_build_lib.CommandResult(
        output='\n%s\n\n' % chrome_lkgm_expected)

    gclient.GetBaseURLs().AndReturn((svn_url, None, 'foo'))
    mox_args = [mox.In(full_svn_url)]
    if chrome_revision:
      mox_args += [mox.In(str(chrome_revision))]

    svn_cmd = mox.And(*mox_args)
    cros_build_lib.RunCommandCaptureOutput(svn_cmd).AndReturn(result)

    self.mox.ReplayAll()
    chrome_lkgm_returned = commands.GetChromeLKGM(chrome_revision)
    self.assertEqual(chrome_lkgm_returned, chrome_lkgm_expected)
    self.mox.VerifyAll()

  def testChromeLKGM(self):
    """Verifies that we can get the chrome lkgm without a chrome revision."""
    self._TestChromeLKGM(None)

  def testChromeLKGMWithRevision(self):
    """Verifies that we can get the chrome lkgm with a chrome revision."""
    self._TestChromeLKGM(1234)


if __name__ == '__main__':
  cros_test_lib.main()
