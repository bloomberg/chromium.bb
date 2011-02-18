#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for cbuildbot.  Needs to be run inside of chroot for mox."""

import __builtin__
import mox
import os
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import cbuildbot
from chromite.lib.cros_build_lib import ReinterpretPathForChroot


class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)
    # Always stub RunCommmand out as we use it in every method.
    self.mox.StubOutWithMock(cbuildbot, 'OldRunCommand')
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
        ReinterpretPathForChroot(p) for p in self._overlays
    ]

  def testParseRevisionString(self):
    """Test whether _ParseRevisionString parses string correctly."""
    return_array = cbuildbot._ParseRevisionString(self._test_string,
                                                  self._test_dict)
    self.assertEqual(len(return_array), 3)
    self.assertTrue('chromeos-base/kernel', '12345test' in return_array)
    self.assertTrue('dev-util/perf', '12345test' in return_array)
    self.assertTrue('chromos-base/libcros', '12345test' in return_array)

  def testCreateDictionary(self):
    self.mox.StubOutWithMock(cbuildbot, '_GetAllGitRepos')
    self.mox.StubOutWithMock(cbuildbot, '_GetCrosWorkOnSrcPath')
    cbuildbot._GetAllGitRepos(mox.IgnoreArg()).AndReturn(self._test_repos)
    cbuildbot.OldRunCommand(mox.IgnoreArg(),
                            cwd='%s/src/scripts' % self._buildroot,
                            redirect_stdout=True,
                            redirect_stderr=True,
                            enter_chroot=True,
                            print_cmd=False).AndReturn(
                                self._test_cros_workon_packages)
    cbuildbot._GetCrosWorkOnSrcPath(
        self._buildroot, self._test_board, 'chromeos-base/kernel').AndReturn(
            '/home/test/third_party/kernel/files')
    cbuildbot._GetCrosWorkOnSrcPath(
        self._buildroot, self._test_board,
        'chromeos-base/chromeos-login').AndReturn(
            '/home/test/platform/login_manager')
    self.mox.ReplayAll()
    repo_dict = cbuildbot._CreateRepoDictionary(self._buildroot,
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

  #  cbuildbot._CreateRepoDictionary(self._buildroot,
  #                                  self._test_board).AndReturn(
  #                                      self._test_dict)
  #  cbuildbot._ParseRevisionString(self._test_string,
  #                                 self._test_dict).AndReturn(
  #                                     self._test_parsed_string_array)
  #  cbuildbot._UprevFromRevisionList(self._buildroot,
  #                                   self._test_parsed_string_array)
  #  self.mox.ReplayAll()
  #  cbuildbot._UprevPackages(self._buildroot, self._revision_file,
  #                           self._test_board)
  #  self.mox.VerifyAll()

  def testArchiveTestResults(self):
    """Test if we can archive the latest results dir to Google Storage."""
    # Set vars for call.
    buildroot = '/fake_dir'
    board = 'fake-board'
    test_results_dir = 'fake_results_dir'
    gsutil_path = '/fake/gsutil/path'
    archive_dir = 1234
    acl = 'fake_acl'
    num_retries = 5

    # Convenience variables to make archive easier to understand.
    path_to_results = os.path.join(buildroot, 'chroot', test_results_dir)
    path_to_image = os.path.join(buildroot, 'src', 'build', 'images', board,
                                 'latest', 'chromiumos_qemu_image.bin')

    cbuildbot.OldRunCommand(['sudo', 'chmod', '-R', '+r', path_to_results])
    cbuildbot.OldRunCommand([gsutil_path, 'cp', '-R', path_to_results,
                             archive_dir], num_retries=num_retries)
    cbuildbot.OldRunCommand([gsutil_path, 'setacl', acl, archive_dir])
    cbuildbot.OldRunCommand(['gzip', '-f', '--fast', path_to_image])
    cbuildbot.OldRunCommand([gsutil_path, 'cp', path_to_image + '.gz',
                             archive_dir], num_retries=num_retries)

    self.mox.ReplayAll()
    cbuildbot._ArchiveTestResults(buildroot, board, test_results_dir,
                                  gsutil_path, archive_dir, acl)
    self.mox.VerifyAll()

  # TODO(sosa):  Remove once we un-comment above.
  def testUprevPackages(self):
    """Test if we get actual revisions in revisions.pfq."""
    self.mox.StubOutWithMock(__builtin__, 'open')

    # Mock out file interaction.
    m_file = self.mox.CreateMock(file)
    __builtin__.open(self._revision_file).AndReturn(m_file)
    m_file.read().AndReturn(self._test_string)
    m_file.close()

    drop_file = cbuildbot._PACKAGE_FILE % {'buildroot': self._buildroot}
    cbuildbot.OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--all',
         '--board=%s' % self._test_board,
         '--overlays=%s' % ':'.join(self._chroot_overlays),
         '--tracking_branch=cros/master',
         '--drop_file=%s' % ReinterpretPathForChroot(drop_file),
         'commit'],
        cwd='%s/src/scripts' % self._buildroot,
        enter_chroot=True)

    self.mox.ReplayAll()
    cbuildbot._UprevPackages(self._buildroot, self.tracking_branch,
                             self._revision_file, self._test_board,
                             self._overlays)
    self.mox.VerifyAll()

  def testUprevAllPackages(self):
    """Test if we get None in revisions.pfq indicating Full Builds."""
    self.mox.StubOutWithMock(__builtin__, 'open')

    # Mock out file interaction.
    m_file = self.mox.CreateMock(file)
    __builtin__.open(self._revision_file).AndReturn(m_file)
    m_file.read().AndReturn('None')
    m_file.close()

    drop_file = cbuildbot._PACKAGE_FILE % {'buildroot': self._buildroot}
    cbuildbot.OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--all',
         '--board=%s' % self._test_board,
         '--overlays=%s' % ':'.join(self._chroot_overlays),
         '--tracking_branch=cros/master',
         '--drop_file=%s' % ReinterpretPathForChroot(drop_file),
         'commit'],
        cwd='%s/src/scripts' % self._buildroot,
        enter_chroot=True)

    self.mox.ReplayAll()
    cbuildbot._UprevPackages(self._buildroot, self.tracking_branch,
                             self._revision_file, self._test_board,
                             self._overlays)
    self.mox.VerifyAll()

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    envvar = 'EXAMPLE'
    cbuildbot.OldRunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                            cwd='%s/src/scripts' % self._buildroot,
                            redirect_stdout=True, enter_chroot=True,
                            error_ok=True).AndReturn('RESULT\n')
    self.mox.ReplayAll()
    result = cbuildbot._GetPortageEnvVar(self._buildroot, self._test_board,
                                         envvar)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')

  def testUploadPublicPrebuilts(self):
    """Test _UploadPrebuilts with a public location."""
    binhost = 'http://www.example.com'
    binhosts = [binhost, None]
    check = mox.And(mox.IsA(list), mox.In(binhost), mox.Not(mox.In(None)),
                    mox.In('gs://chromeos-prebuilt'), mox.In('preflight'))
    cbuildbot.OldRunCommand(check, cwd=os.path.dirname(cbuildbot.__file__))
    self.mox.ReplayAll()
    cbuildbot._UploadPrebuilts(self._buildroot, self._test_board, 'public',
                               binhosts, 'preflight', None)
    self.mox.VerifyAll()

  def testUploadPrivatePrebuilts(self):
    """Test _UploadPrebuilts with a private location."""
    binhost = 'http://www.example.com'
    binhosts = [binhost, None]
    check = mox.And(mox.IsA(list), mox.In(binhost), mox.Not(mox.In(None)),
                    mox.In('chromeos-images:/var/www/prebuilt/'),
                    mox.In('chrome'))
    cbuildbot.OldRunCommand(check, cwd=os.path.dirname(cbuildbot.__file__))
    self.mox.ReplayAll()
    cbuildbot._UploadPrebuilts(self._buildroot, self._test_board, 'private',
                               binhosts, 'chrome', 'tot')
    self.mox.VerifyAll()


if __name__ == '__main__':
  unittest.main()
