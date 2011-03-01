#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for commands.  Needs to be run inside of chroot for mox."""

import __builtin__
import mox
import os
import shutil
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import chromite.buildbot.cbuildbot_commands as commands
import chromite.lib.cros_build_lib as cros_lib

class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cros_lib, 'OldRunCommand')
    self.tracking_branch = 'cros/master'
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

  def testArchiveTestResults(self):
    """Test if we can archive the latest results dir to Google Storage."""
    # Set vars for call.
    self.mox.StubOutWithMock(shutil, 'rmtree')
    buildroot = '/fake_dir'
    archive_tarball = os.path.join(buildroot, 'test_results.tgz')
    test_results_dir = 'fake_results_dir'

    # Convenience variables to make archive easier to understand.
    path_to_results = os.path.join(buildroot, 'chroot', test_results_dir)

    cros_lib.OldRunCommand(['sudo', 'chmod', '-R', 'a+rw', path_to_results])
    cros_lib.OldRunCommand(['tar', 'czf', archive_tarball, path_to_results])
    shutil.rmtree(path_to_results)
    self.mox.ReplayAll()
    return_ball = commands.ArchiveTestResults(buildroot, test_results_dir)
    self.mox.VerifyAll()
    self.assertEqual(return_ball, archive_tarball)


  def testUprevAllPackages(self):
    """Test if we get None in revisions.pfq indicating Full Builds."""
    drop_file = commands._PACKAGE_FILE % {'buildroot': self._buildroot}
    cros_lib.OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--all',
         '--board=%s' % self._test_board,
         '--overlays=%s' % ':'.join(self._chroot_overlays),
         '--tracking_branch=cros/master',
         '--drop_file=%s' % cros_lib.ReinterpretPathForChroot(drop_file),
         'commit'],
        cwd='%s/src/scripts' % self._buildroot,
        enter_chroot=True)

    self.mox.ReplayAll()
    commands.UprevPackages(self._buildroot, self.tracking_branch,
                            self._test_board, self._overlays)
    self.mox.VerifyAll()

  def testUploadPublicPrebuilts(self):
    """Test _UploadPrebuilts with a public location."""
    binhost = 'http://www.example.com'
    binhosts = [binhost, None]
    check = mox.And(mox.IsA(list), mox.In(binhost), mox.Not(mox.In(None)),
                    mox.In('gs://chromeos-prebuilt'), mox.In('preflight'))
    cros_lib.OldRunCommand(check, cwd=os.path.dirname(commands.__file__))
    self.mox.ReplayAll()
    commands.UploadPrebuilts(self._buildroot, self._test_board, 'public',
                              binhosts, 'preflight', None)
    self.mox.VerifyAll()

  def testUploadPrivatePrebuilts(self):
    """Test _UploadPrebuilts with a private location."""
    binhost = 'http://www.example.com'
    binhosts = [binhost, None]
    check = mox.And(mox.IsA(list), mox.In(binhost), mox.Not(mox.In(None)),
                    mox.In('chromeos-images:/var/www/prebuilt/'),
                    mox.In('chrome'))
    cros_lib.OldRunCommand(check, cwd=os.path.dirname(commands.__file__))
    self.mox.ReplayAll()
    commands.UploadPrebuilts(self._buildroot, self._test_board, 'private',
                             binhosts, 'chrome', 'tot')
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
