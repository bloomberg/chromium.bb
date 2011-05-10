#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_chrome_as_stable.py."""

import mox
import os
import shutil
import sys
import tempfile
import unittest
import urllib

import constants
sys.path.append(constants.CROSUTILS_LIB_DIR)
import cros_mark_as_stable
import cros_mark_chrome_as_stable

unstable_data = 'KEYWORDS=~x86 ~arm'
stable_data = 'KEYWORDS=x86 arm'

def _TouchAndWrite(path, data=None):
  """Writes data (if it exists) to the file specified by the path."""
  fh = open(path, 'w')
  if data:
    fh.write(data)

  fh.close()


class CrosMarkChromeAsStable(mox.MoxTestBase):

  def setUp(self):
    """Setup vars and create mock dir."""
    mox.MoxTestBase.setUp(self)
    self.tmp_overlay = tempfile.mkdtemp(prefix='chromiumos-overlay')
    self.mock_chrome_dir = os.path.join(self.tmp_overlay, 'chromeos-base',
                                        'chromeos-chrome')
    os.makedirs(self.mock_chrome_dir)

    self.unstable = os.path.join(self.mock_chrome_dir,
                                 'chromeos-chrome-9999.ebuild')
    self.sticky_branch = '8.0.224'
    self.sticky_version = '%s.503' % self.sticky_branch
    self.sticky = os.path.join(self.mock_chrome_dir,
                               'chromeos-chrome-%s.ebuild' %
                                   self.sticky_version)
    self.sticky_rc_version = '%s.504' % self.sticky_branch
    self.sticky_rc = os.path.join(self.mock_chrome_dir,
                                  'chromeos-chrome-%s_rc-r1.ebuild' %
                                      self.sticky_rc_version)
    self.latest_stable_version = '8.0.300.1'
    self.latest_stable = os.path.join(self.mock_chrome_dir,
                                      'chromeos-chrome-%s_rc-r2.ebuild' %
                                          self.latest_stable_version)
    self.tot_stable_version = '9.0.305.0'
    self.tot_stable = os.path.join(self.mock_chrome_dir,
                                   'chromeos-chrome-%s_alpha-r1.ebuild' %
                                       self.tot_stable_version)

    self.sticky_new_rc_version = '%s.520' % self.sticky_branch
    self.sticky_new_rc = os.path.join(self.mock_chrome_dir,
                                      'chromeos-chrome-%s_rc-r1.ebuild' %
                                          self.sticky_new_rc_version)
    self.latest_new_version = '9.0.305.1'
    self.latest_new = os.path.join(self.mock_chrome_dir,
                                      'chromeos-chrome-%s_rc-r1.ebuild' %
                                          self.latest_new_version)
    self.tot_new_version = '9.0.306.0'
    self.tot_new = os.path.join(self.mock_chrome_dir,
                                      'chromeos-chrome-%s_alpha-r1.ebuild' %
                                          self.tot_new_version)

    _TouchAndWrite(self.unstable, unstable_data)
    _TouchAndWrite(self.sticky, stable_data)
    _TouchAndWrite(self.sticky_rc, stable_data)
    _TouchAndWrite(self.latest_stable, stable_data)
    _TouchAndWrite(self.tot_stable, stable_data)

  def tearDown(self):
    """Cleans up mock dir."""
    shutil.rmtree(self.tmp_overlay)

  def testFindChromeCandidates(self):
    """Test creation of stable ebuilds from mock dir."""
    unstable, stable_ebuilds = cros_mark_chrome_as_stable.FindChromeCandidates(
        self.mock_chrome_dir)

    self.assertEqual(unstable.ebuild_path, self.unstable)
    self.assertEqual(len(stable_ebuilds), 4)
    self.assertTrue(cros_mark_chrome_as_stable.ChromeEBuild(self.sticky) in
                    stable_ebuilds)
    self.assertTrue(cros_mark_chrome_as_stable.ChromeEBuild(self.sticky_rc) in
                    stable_ebuilds)
    self.assertTrue(cros_mark_chrome_as_stable.ChromeEBuild(self.latest_stable)
                    in stable_ebuilds)
    self.assertTrue(cros_mark_chrome_as_stable.ChromeEBuild(self.tot_stable) in
                    stable_ebuilds)

  def _GetStableEBuilds(self):
    """Common helper to create a list of stable ebuilds."""
    return [
        cros_mark_chrome_as_stable.ChromeEBuild(self.sticky),
        cros_mark_chrome_as_stable.ChromeEBuild(self.sticky_rc),
        cros_mark_chrome_as_stable.ChromeEBuild(self.latest_stable),
        cros_mark_chrome_as_stable.ChromeEBuild(self.tot_stable),
    ]

  def testTOTFindChromeUprevCandidate(self):
    """Tests if we can find tot uprev candidate from our mock dir data."""
    stable_ebuilds = self._GetStableEBuilds()

    candidate = cros_mark_chrome_as_stable.FindChromeUprevCandidate(
        stable_ebuilds, cros_mark_chrome_as_stable.TIP_OF_TRUNK,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.tot_stable)

  def testLatestFindChromeUprevCandidate(self):
    """Tests if we can find latest uprev candidate from our mock dir data."""
    stable_ebuilds = self._GetStableEBuilds()

    candidate = cros_mark_chrome_as_stable.FindChromeUprevCandidate(
        stable_ebuilds, cros_mark_chrome_as_stable.LATEST_RELEASE,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.latest_stable)

  def testStickyFindChromeUprevCandidate(self):
    """Tests if we can find sticky uprev candidate from our mock dir data."""
    stable_ebuilds = self._GetStableEBuilds()

    candidate = cros_mark_chrome_as_stable.FindChromeUprevCandidate(
        stable_ebuilds, cros_mark_chrome_as_stable.STICKY,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.sticky_rc)

  def testGetTipOfTrunkSvnRevision(self):
    """Tests if we can get the latest svn revision from TOT."""
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'info', cros_mark_chrome_as_stable._GetSvnUrl()],
        redirect_stdout=True).AndReturn(
            'Some Junk 2134\nRevision: 12345\nOtherInfo: test_data')
    self.mox.ReplayAll()
    revision = cros_mark_chrome_as_stable._GetTipOfTrunkSvnRevision()
    self.mox.VerifyAll()
    self.assertEquals(revision, '12345')

  def testGetTipOfTrunkVersion(self):
    """Tests if we get the latest version from TOT."""
    path = os.path.join(cros_mark_chrome_as_stable._GetSvnUrl(), 'src',
                        'chrome', 'VERSION')
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'cat', path], redirect_stdout=True,
        error_message=mox.IsA(str)).AndReturn('A=8\nB=0\nC=256\nD=0')

    self.mox.ReplayAll()
    version = cros_mark_chrome_as_stable._GetTipOfTrunkVersion()
    self.mox.VerifyAll()
    self.assertEquals(version, '8.0.256.0')

  def testGetLatestRelease(self):
    """Tests if we can find the latest release from our mock url data."""
    test_data = '\n'.join(['7.0.224.1/',
                           '7.0.224.2/',
                           '8.0.365.5/',
                           'LATEST.txt'])
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', 'svn://svn.chromium.org/chrome/releases'],
        redirect_stdout=True).AndReturn('some_data')
    cros_mark_chrome_as_stable.RunCommand(
        ['sort', '--version-sort'], input='some_data',
        redirect_stdout=True).AndReturn(test_data)
    self.mox.ReplayAll()
    release = cros_mark_chrome_as_stable._GetLatestRelease()
    self.mox.VerifyAll()
    self.assertEqual('8.0.365.5', release)

  def testGetLatestStickyRelease(self):
    """Tests if we can find the latest sticky release from our mock url data."""
    test_data = '\n'.join(['7.0.222.1/',
                           '8.0.224.2/',
                           '8.0.365.5/',
                           'LATEST.txt'])
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', 'svn://svn.chromium.org/chrome/releases'],
        redirect_stdout=True).AndReturn('some_data')
    cros_mark_chrome_as_stable.RunCommand(
        ['sort', '--version-sort'], input='some_data',
        redirect_stdout=True).AndReturn(test_data)
    self.mox.ReplayAll()
    release = cros_mark_chrome_as_stable._GetLatestRelease(self.sticky_branch)
    self.mox.VerifyAll()
    self.assertEqual('8.0.224.2', release)

  def testStickyEBuild(self):
    """Tests if we can find the sticky ebuild from our mock directories."""
    stable_ebuilds = self._GetStableEBuilds()
    sticky_ebuild = cros_mark_chrome_as_stable._GetStickyEBuild(
        stable_ebuilds)
    self.assertEqual(sticky_ebuild.chrome_version, self.sticky_version)

  def testChromeEBuildInit(self):
    """Tests if the chrome_version is set correctly in a ChromeEBuild."""
    ebuild = cros_mark_chrome_as_stable.ChromeEBuild(self.sticky)
    self.assertEqual(ebuild.chrome_version, self.sticky_version)

  def _CommonMarkAsStableTest(self, chrome_rev, new_version, old_ebuild_path,
                              new_ebuild_path, commit_string_indicator):
    """Common function used for test functions for MarkChromeEBuildAsStable.

    This function stubs out others calls, and runs MarkChromeEBuildAsStable
    with the specified args.

    Args:
      chrome_rev: standard chrome_rev argument
      new_version: version we are revving up to
      old_ebuild_path: path to the stable ebuild
      new_ebuild_path: path to the to be created path
      commit_string_indicator: a string that the commit message must contain
    """
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    self.mox.StubOutWithMock(cros_mark_as_stable.EBuildStableMarker,
                             'CommitChange')
    stable_candidate = cros_mark_chrome_as_stable.ChromeEBuild(old_ebuild_path)
    unstable_ebuild = cros_mark_chrome_as_stable.ChromeEBuild(self.unstable)
    sticky_ebuild = cros_mark_chrome_as_stable.ChromeEBuild(self.sticky)
    chrome_version = new_version
    commit = None
    overlay_dir = self.mock_chrome_dir

    cros_mark_chrome_as_stable.RunCommand(['git', 'add', new_ebuild_path])
    cros_mark_chrome_as_stable.RunCommand(['git', 'rm', old_ebuild_path])
    cros_mark_as_stable.EBuildStableMarker.CommitChange(
        mox.StrContains(commit_string_indicator))

    self.mox.ReplayAll()
    cros_mark_chrome_as_stable.MarkChromeEBuildAsStable(
        stable_candidate, unstable_ebuild, chrome_rev, chrome_version, commit,
        overlay_dir, sticky_ebuild)
    self.mox.VerifyAll()

  def testStickyMarkAsStable(self):
    """Tests to see if we can mark chrome as stable for a new sticky release."""
    self._CommonMarkAsStableTest(cros_mark_chrome_as_stable.STICKY,
                                 self.sticky_new_rc_version, self.sticky_rc,
                                 self.sticky_new_rc, 'stable_release')

  def testLatestMarkAsStable(self):
    """Tests to see if we can mark chrome for a latest release."""
    self._CommonMarkAsStableTest(cros_mark_chrome_as_stable.LATEST_RELEASE,
                                 self.latest_new_version, self.latest_stable,
                                 self.latest_new, 'latest_release')

  def testTotMarkAsStable(self):
    """Tests to see if we can mark chrome for tot."""
    self._CommonMarkAsStableTest(cros_mark_chrome_as_stable.TIP_OF_TRUNK,
                                 self.tot_new_version, self.tot_stable,
                                 self.tot_new, 'tot')


if __name__ == '__main__':
  unittest.main()
