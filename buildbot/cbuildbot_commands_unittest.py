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
    archive_tarball = os.path.join(buildroot, 'test_results.tgz')
    test_results_dir = 'fake_results_dir'

    # Convenience variables to make archive easier to understand.
    path_to_results = os.path.join(buildroot, 'chroot', test_results_dir)

    cros_lib.OldRunCommand(['sudo', 'chmod', '-R', 'a+rw', path_to_results],
                           print_cmd=False)
    cros_lib.OldRunCommand(['tar',
                            'czf',
                            archive_tarball,
                            '--directory=%s' % path_to_results,
                            '.'])
    shutil.rmtree(path_to_results)
    self.mox.ReplayAll()
    commands.ArchiveTestResults(buildroot, test_results_dir, None, False)
    self.mox.VerifyAll()

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
                   skip_toolchain_update=False)
    self.mox.VerifyAll()

  def testBuildMaximum(self):
    """Base case where Build is called with all options (except extra_evn)."""
    buildroot = '/bob/'
    arg_test = mox.SameElementsAs(['./build_packages',
                                   '--fast',
                                   '--board=x86-generic',
                                   '--skip_toolchain_update'])
    cros_lib.RunCommand(arg_test,
                        cwd=mox.StrContains(buildroot),
                        enter_chroot=True,
                        extra_env={'EXTRA_BOARD_FLAGS': '--rebuilt-binaries'})
    self.mox.ReplayAll()
    commands.Build(buildroot=buildroot,
                   board='x86-generic',
                   fast=True,
                   build_autotest=True,
                   usepkg=True,
                   skip_toolchain_update=True)
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
                   extra_env=extra)
    self.mox.VerifyAll()

  def testLegacyArchiveBuildMinimal(self):
    """Test Legacy Archive Command, with minimal values."""
    buildroot = '/bob'

    buildconfig = {}
    buildconfig['board'] = 'config_board'
    buildconfig['gs_path'] = None
    buildconfig['factory_test_mod'] = False
    buildconfig['test_mod'] = False
    buildconfig['factory_install_mod'] = False
    buildconfig['useflags'] = None
    buildconfig['archive_build_debug'] = False

    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(socket, 'gethostname')

    cros_lib.RunCommand(['./archive_build.sh',
                         '--set_version', 'myversion',
                         '--to', '/var/www/archive/bot_id',
                         '--keep_max', '3',
                         '--board', buildconfig['board'],
                         '--noarchive_debug',
                         '--notest_mod'],
                        cwd='/bob/src/scripts')

    self.mox.ReplayAll()
    commands.LegacyArchiveBuild(buildroot,
                                'bot_id',
                                buildconfig,
                                None,
                                'myversion',
                                '/var/www/archive',
                                debug=False)
    self.mox.VerifyAll()

  def testLegacyArchiveBuildMaximum(self):
    """Test Legacy Archive Command, with all values."""
    buildroot = '/bob'

    buildconfig = {}
    buildconfig['board'] = 'config_board'
    buildconfig['gs_path'] = 'gs://gs/path'
    buildconfig['factory_test_mod'] = True
    buildconfig['test_mod'] = True
    buildconfig['factory_install_mod'] = True
    buildconfig['useflags'] = ['use_a', 'use_b']
    buildconfig['archive_build_debug'] = True

    self.mox.StubOutWithMock(os.path, 'exists')
    self.mox.StubOutWithMock(socket, 'gethostname')

    cros_lib.RunCommand(['./archive_build.sh',
                         '--set_version', 'myversion',
                         '--to', '/var/www/archive/bot_id',
                         '--keep_max', '3',
                         '--board', 'config_board',
                         '--gsutil_archive', 'gs://gs/path',
                         '--acl', '/home/chrome-bot/slave_archive_acl',
                         '--gsutil', '/b/scripts/slave/gsutil',
                         '--factory_test_mod',
                         '--debug',
                         '--factory_install_mod',
                         '--useflags', 'use_a use_b'],
                        cwd='/bob/src/scripts')

    self.mox.ReplayAll()
    commands.LegacyArchiveBuild(buildroot,
                                'bot_id',
                                buildconfig,
                                buildconfig['gs_path'],
                                'myversion',
                                '/var/www/archive',
                                debug=True)
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
