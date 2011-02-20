#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
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
import chromite.buildbot.cbuildbot_stages as stages
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

  def LegacyTestParseRevisionString(self):
    """Test whether _ParseRevisionString parses string correctly."""
    return_array = commands._ParseRevisionString(self._test_string,
                                                  self._test_dict)
    self.assertEqual(len(return_array), 3)
    self.assertTrue('chromeos-base/kernel', '12345test' in return_array)
    self.assertTrue('dev-util/perf', '12345test' in return_array)
    self.assertTrue('chromos-base/libcros', '12345test' in return_array)

  def LegacyTestCreateDictionary(self):
    self.mox.StubOutWithMock(cbuildbot, '_GetAllGitRepos')
    self.mox.StubOutWithMock(cbuildbot, '_GetCrosWorkOnSrcPath')
    commands._GetAllGitRepos(mox.IgnoreArg()).AndReturn(self._test_repos)
    cros_lib.OldRunCommand(mox.IgnoreArg(),
                            cwd='%s/src/scripts' % self._buildroot,
                            redirect_stdout=True,
                            redirect_stderr=True,
                            enter_chroot=True,
                            print_cmd=False).AndReturn(
                                self._test_cros_workon_packages)
    commands._GetCrosWorkOnSrcPath(
        self._buildroot, self._test_board, 'chromeos-base/kernel').AndReturn(
            '/home/test/third_party/kernel/files')
    commands._GetCrosWorkOnSrcPath(
        self._buildroot, self._test_board,
        'chromeos-base/chromeos-login').AndReturn(
            '/home/test/platform/login_manager')
    self.mox.ReplayAll()
    repo_dict = commands._CreateRepoDictionary(self._buildroot,
                                                self._test_board)
    self.assertEqual(repo_dict['kernel'], ['chromeos-base/kernel'])
    self.assertEqual(repo_dict['login_manager'],
                     ['chromeos-base/chromeos-login'])
    self.mox.VerifyAll()

  # TODO(sosa): Re-add once we use cros_mark vs. cros_mark_all.
  #def testUprevPackages(self):
  #  """Test if we get actual revisions in revisions.pfq."""
  #  self.mox.StubOutWithMock(cbuildbot, '_CreateRepoDictionary')
  #  self.mox.StubOutWithMock(cbuildbot, '_ParseRevisionString')
  #  self.mox.StubOutWithMock(cbuildbot, '_UprevFromRevisionList')
  #  self.mox.StubOutWithMock(__builtin__, 'open')

  #  # Mock out file interaction.
  #  m_file = self.mox.CreateMock(file)
  #  __builtin__.open(self._revision_file).AndReturn(m_file)
  #  m_file.read().AndReturn(self._test_string)
  #  m_file.close()

  #  commands._CreateRepoDictionary(self._buildroot,
  #                                  self._test_board).AndReturn(
  #                                      self._test_dict)
  #  commands._ParseRevisionString(self._test_string,
  #                                 self._test_dict).AndReturn(
  #                                     self._test_parsed_string_array)
  #  commands._UprevFromRevisionList(self._buildroot,
  #                                   self._test_parsed_string_array)
  #  self.mox.ReplayAll()
  #  commands.UprevPackages(self._buildroot, self._revision_file,
  #                           self._test_board)
  #  self.mox.VerifyAll()

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

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    envvar = 'EXAMPLE'
    cros_lib.OldRunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                           cwd='%s/src/scripts' % self._buildroot,
                           redirect_stdout=True, enter_chroot=True,
                           error_ok=True).AndReturn('RESULT\n')
    self.mox.ReplayAll()
    result = stages._GetPortageEnvVar(self._buildroot, self._test_board,
                                      envvar)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')

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
