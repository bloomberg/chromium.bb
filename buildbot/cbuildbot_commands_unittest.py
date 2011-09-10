#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import mox
import os
import shutil
import socket
import sys
import tempfile
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import chromite.buildbot.cbuildbot_commands as commands
import chromite.lib.cros_build_lib as cros_lib
import chromite.buildbot.repository as repository


class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    self.mox.StubOutWithMock(cros_lib, 'RunCommand')
    self.mox.StubOutWithMock(repository, 'FixExternalRepoPushUrls')
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
        cros_lib.ReinterpretPathForChroot(p) for p in self._overlays
    ]
    self._CWD = os.path.dirname(os.path.realpath(__file__))

  def testArchiveTestResults(self):
    """Test if we can archive the latest results dir to Google Storage."""
    # Set vars for call.
    self.mox.StubOutWithMock(shutil, 'rmtree')
    buildroot = '/fake_dir'
    test_tarball = os.path.join(buildroot, 'test_results.tgz')
    test_results_dir = 'fake_results_dir'

    # Convenience variables to make archive easier to understand.
    path_to_results = os.path.join(buildroot, 'chroot', test_results_dir)

    cros_lib.OldRunCommand(['sudo', 'chmod', '-R', 'a+rw', path_to_results],
                           print_cmd=False)
    cros_lib.OldRunCommand(['tar',
                            'czf',
                            test_tarball,
                            '--directory=%s' % path_to_results,
                            '.'])
    shutil.rmtree(path_to_results)
    self.mox.ReplayAll()
    commands.ArchiveTestResults(buildroot, test_results_dir)
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

    self.mox.StubOutWithMock(tempfile, 'mkdtemp')
    tempfile.mkdtemp(dir=mox.IgnoreArg(), prefix=mox.IgnoreArg()). \
        AndReturn(temp_dir)
    self.mox.StubOutWithMock(os, 'walk')
    dump_file_dir, dump_file_name = os.path.split(dump_file)
    os.walk(mox.IgnoreArg()).AndReturn([(dump_file_dir, [''],
                                       [dump_file_name])])
    self.mox.StubOutWithMock(cros_lib, 'ReinterpretPathForChroot')
    cros_lib.ReinterpretPathForChroot(mox.IgnoreArg()).AndReturn(dump_file)
    self.mox.StubOutWithMock(os, 'unlink')
    self.mox.StubOutWithMock(shutil, 'rmtree')

    cros_lib.RunCommand(['gzip', '-df', gzipped_test_tarball])
    cros_lib.RunCommand(
        ['tar',
         'xf',
         test_tarball,
         '--directory=%s' % temp_dir,
         '--wildcards', '*.dmp'],
         error_ok=True,
         exit_code=True,
         redirect_stderr=True).AndReturn(cros_lib.CommandResult())
    cros_lib.RunCommand(['minidump_stackwalk',
                         dump_file,
                         symbol_dir, ],
                        cwd=cwd,
                        enter_chroot=True,
                        error_ok=True,
                        log_stdout_to_file='%s.txt' % dump_file,
                        redirect_stderr=True)
    cros_lib.RunCommand(['tar',
                         'uf',
                         test_tarball,
                         '--directory=%s' % temp_dir,
                         '.'])
    cros_lib.RunCommand('gzip -c %s > %s' %
                        (test_tarball, gzipped_test_tarball),
                        shell=True)
    os.unlink(test_tarball)
    shutil.rmtree(temp_dir)

    self.mox.ReplayAll();
    commands.GenerateMinidumpStackTraces(buildroot, board,
                                         gzipped_test_tarball)
    self.mox.VerifyAll();

  def testUprevAllPackages(self):
    """Test if we get None in revisions.pfq indicating Full Builds."""
    drop_file = commands._PACKAGE_FILE % {'buildroot': self._buildroot}
    cros_lib.OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--all',
         '--board=%s' % self._test_board,
         '--overlays=%s' % ':'.join(self._chroot_overlays),
         '--drop_file=%s' % cros_lib.ReinterpretPathForChroot(drop_file),
         'commit'],
        cwd='%s/src/scripts' % self._buildroot,
        enter_chroot=True)

    self.mox.ReplayAll()
    commands.UprevPackages(self._buildroot,
                            self._test_board,
                            self._overlays)
    self.mox.VerifyAll()

  def testUploadPublicPrebuilts(self):
    """Test _UploadPrebuilts with a public location."""
    buildnumber = 4
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-prebuilt'),
                    mox.In('binary'))
    cros_lib.OldRunCommand(check, cwd=os.path.dirname(commands.__file__))
    self.mox.ReplayAll()
    commands.UploadPrebuilts(self._buildroot, self._test_board, 'public',
                             'binary', None, buildnumber)
    self.mox.VerifyAll()

  def testUploadPrivatePrebuilts(self):
    """Test _UploadPrebuilts with a private location."""
    buildnumber = 4
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-%s/%s/%d/prebuilts/' %
                           (self._test_board, 'full', buildnumber)),
                    mox.In('full'))
    cros_lib.OldRunCommand(check, cwd=os.path.dirname(commands.__file__))
    self.mox.ReplayAll()
    commands.UploadPrebuilts(self._buildroot, self._test_board, 'private',
                             'full', None, buildnumber)
    self.mox.VerifyAll()

  def testChromePrebuilts(self):
    """Test _UploadPrebuilts for Chrome prebuilts."""
    buildnumber = 4
    check = mox.And(mox.IsA(list),
                    mox.In('gs://chromeos-prebuilt'),
                    mox.In('chrome'))
    cros_lib.OldRunCommand(check, cwd=os.path.dirname(commands.__file__))
    self.mox.ReplayAll()
    commands.UploadPrebuilts(self._buildroot, self._test_board, 'public',
                             'chrome', 'tot', buildnumber)
    self.mox.VerifyAll()


  def testBuildMinimal(self):
    """Base case where Build is called with minimal options."""
    buildroot = '/bob/'
    arg_test = mox.SameElementsAs(['./build_packages',
                                   '--fast',
                                   '--nowithautotest',
                                   '--nousepkg',
                                   '--board=x86-generic'])
    cros_lib.RunCommand(arg_test,
                        cwd=mox.StrContains(buildroot),
                        enter_chroot=True,
                        extra_env={})
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   build_autotest=False,
                   fast=True,
                   usepkg=False,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   )
    self.mox.VerifyAll()

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_evn)."""
    buildroot = '/bob/'
    arg_test = mox.SameElementsAs(['./build_packages',
                                   '--fast',
                                   '--board=x86-generic',
                                   '--skip_toolchain_update',
                                   '--nowithdebug'])
    cros_lib.RunCommand(arg_test,
                        cwd=mox.StrContains(buildroot),
                        enter_chroot=True,
                        extra_env={})
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   fast=True,
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
    cros_lib.RunCommand(
      mox.IgnoreArg(),
      cwd=mox.StrContains(buildroot),
      enter_chroot=True,
      extra_env=mox.And(
        mox.ContainsKeyValue('A', 'Av'), mox.ContainsKeyValue('B', 'Bv')))
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   build_autotest=False,
                   fast=False,
                   usepkg=False,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   extra_env=extra)
    self.mox.VerifyAll()

  def testUploadSymbols(self):
    """Test UploadSymbols Command."""
    buildroot = '/bob'
    board = 'board_name'

    cros_lib.RunCommand(['./upload_symbols',
                         '--board=board_name',
                         '--yes',
                         '--verbose',
                         '--official_build'],
                        cwd='/bob/src/scripts',
                        error_ok=True,
                        enter_chroot=True)

    cros_lib.RunCommand(['./upload_symbols',
                         '--board=board_name',
                         '--yes',
                         '--verbose'],
                        cwd='/bob/src/scripts',
                        error_ok=True,
                        enter_chroot=True)

    self.mox.ReplayAll()
    commands.UploadSymbols(buildroot, board, official=True)
    commands.UploadSymbols(buildroot, board, official=False)
    self.mox.VerifyAll()

  def testPushImages(self):
    """Test UploadSymbols Command."""
    buildroot = '/bob'
    board = 'board_name'
    branch_name = 'branch_name'
    archive_dir = '/archive/dir'

    cros_lib.RunCommand(['./pushimage',
                         '--board=board_name',
                         '--branch=branch_name',
                         '/archive/dir'],
                        cwd=mox.StrContains('crostools'))

    self.mox.ReplayAll()
    commands.PushImages(buildroot, board, branch_name, archive_dir)
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
