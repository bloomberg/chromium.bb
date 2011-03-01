# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the unittests for deprecated commands.

# TODO(sosa): Leaving these in case we want to revive this commands later.
Otherwise will delete later.
"""

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
    self.assertTrue(['chromeos-base/kernel', '12345test'] in return_array)
    self.assertTrue(['dev-util/perf', '12345test'] in return_array)
    self.assertTrue(['chromos-base/libcros', '12345test'] in return_array)

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
