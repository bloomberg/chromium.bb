#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for cros_mark_chrome_as_stable.py."""

# run with:
#    cros_sdk ../../chromite/buildbot/cros_mark_chrome_as_stable_unittest.py

import mox
import os
import shutil
import sys
import tempfile
import unittest

import constants
sys.path.append(constants.CROSUTILS_LIB_DIR)
import cros_mark_as_stable
import cros_mark_chrome_as_stable

# pylint: disable=W0212,R0904
unstable_data = 'KEYWORDS=~x86 ~arm'
stable_data = 'KEYWORDS=x86 arm'
fake_svn_rev = '12345'
new_fake_svn_rev = '23456'

class MockCommandResult(object):
  """An object to store various attributes of a child process."""

  def __init__(self, returncode=None, error=None, output=None, cmd=None):
    self.cmd = cmd
    self.error = error
    self.output = output
    self.returncode = returncode


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
    _TouchAndWrite(self.tot_stable,
                   '\n'.join(
                       (stable_data,
                        '%s=%s' % (cros_mark_chrome_as_stable._CHROME_SVN_TAG,
                                   fake_svn_rev))))

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
        stable_ebuilds, constants.CHROME_REV_TOT,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.tot_stable)

  def testLatestFindChromeUprevCandidate(self):
    """Tests if we can find latest uprev candidate from our mock dir data."""
    stable_ebuilds = self._GetStableEBuilds()

    candidate = cros_mark_chrome_as_stable.FindChromeUprevCandidate(
        stable_ebuilds, constants.CHROME_REV_LATEST,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.latest_stable)

  def testStickyFindChromeUprevCandidate(self):
    """Tests if we can find sticky uprev candidate from our mock dir data."""
    stable_ebuilds = self._GetStableEBuilds()

    candidate = cros_mark_chrome_as_stable.FindChromeUprevCandidate(
        stable_ebuilds, constants.CHROME_REV_STICKY,
        self.sticky_branch)

    self.assertEqual(candidate.ebuild_path, self.sticky_rc)

  def testGetTipOfTrunkSvnRevision(self):
    """Tests if we can get the latest svn revision from TOT."""
    A_URL='dorf://mink/delaane/forkat/sertiunu.ortg./desk'
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'info', cros_mark_chrome_as_stable._GetSvnUrl(A_URL)],
        redirect_stdout=True).AndReturn(
            'Some Junk 2134\nRevision: %s\nOtherInfo: test_data' % fake_svn_rev)
    self.mox.ReplayAll()
    revision = cros_mark_chrome_as_stable._GetTipOfTrunkSvnRevision(A_URL)
    self.mox.VerifyAll()
    self.assertEquals(revision, fake_svn_rev)

  def testGetTipOfTrunkVersion(self):
    """Tests if we get the latest version from TOT."""
    ARBITRARY_URL='Pratooey'
    path = os.path.join(cros_mark_chrome_as_stable._GetSvnUrl(ARBITRARY_URL),
                        'src', 'chrome', 'VERSION')
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'info', cros_mark_chrome_as_stable._GetSvnUrl(ARBITRARY_URL)],
        redirect_stdout=True).AndReturn(
            'Some Junk 2134\nRevision: %s\nOtherInfo: test_data' % fake_svn_rev)
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'cat', '-r', fake_svn_rev, path], redirect_stdout=True,
        error_message=mox.IsA(str)).AndReturn('A=8\nB=0\nC=256\nD=0')

    self.mox.ReplayAll()
    version = cros_mark_chrome_as_stable._GetSpecificVersionUrl(ARBITRARY_URL,
                                                                fake_svn_rev)
    self.mox.VerifyAll()
    self.assertEquals(version, '8.0.256.0')

  def testGetLatestRelease(self):
    """Tests if we can find the latest release from our mock url data."""
    ARBITRARY_URL = 'phthp://sores.chromium.org/tqs'
    input_data = ['7.0.224.1/', '7.0.224.2/', '8.0.365.5/', 'LATEST.txt']
    test_data = '\n'.join(input_data)
    sorted_data = '\n'.join(reversed(input_data))
    success = MockCommandResult(returncode=0)
    failure = MockCommandResult(returncode=1)
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', ARBITRARY_URL + '/releases'],
        redirect_stdout=True).AndReturn(test_data)
    cros_mark_chrome_as_stable.RunCommand(
        ['sort', '--version-sort', '-r'], input=test_data,
        redirect_stdout=True).AndReturn(sorted_data)
    # pretend this one is missing to test the skipping logic.
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', ARBITRARY_URL + '/releases/8.0.365.5/DEPS'],
        error_code_ok=True).AndReturn(failure)
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', ARBITRARY_URL + '/releases/7.0.224.2/DEPS'],
        error_code_ok=True).AndReturn(success)
    self.mox.ReplayAll()
    release = cros_mark_chrome_as_stable._GetLatestRelease(ARBITRARY_URL)
    self.mox.VerifyAll()
    self.assertEqual('7.0.224.2', release)

  def testGetLatestStickyRelease(self):
    """Tests if we can find the latest sticky release from our mock url data."""
    ARBITRARY_URL = 'http://src.chromium.org/svn'
    test_data = '\n'.join(['7.0.222.1/',
                           '8.0.224.2/',
                           '8.0.365.5/',
                           'LATEST.txt'])
    success = MockCommandResult(returncode=0)
    self.mox.StubOutWithMock(cros_mark_chrome_as_stable, 'RunCommand')
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', ARBITRARY_URL + '/releases'],
        redirect_stdout=True).AndReturn('some_data')
    cros_mark_chrome_as_stable.RunCommand(
        ['sort', '--version-sort', '-r'], input='some_data',
        redirect_stdout=True).AndReturn(test_data)
    cros_mark_chrome_as_stable.RunCommand(
        ['svn', 'ls', ARBITRARY_URL + '/releases/8.0.224.2/DEPS'],
        error_code_ok=True).AndReturn(success)
    self.mox.ReplayAll()
    release = cros_mark_chrome_as_stable._GetLatestRelease(ARBITRARY_URL,
                                                           '8.0.224')
    self.mox.VerifyAll()
    self.assertEqual('8.0.224.2', release)

  def testGetRevisionFromEBuild(self):
    """Tests that we can recover the svn revision from a ToT chrome ebuild."""
    self.assertEqual(fake_svn_rev,
                     cros_mark_chrome_as_stable.GetRevisionFromEBuild(
                         cros_mark_chrome_as_stable.ChromeEBuild(
                             self.tot_stable)))

  def testLatestChromeRevisionListLink(self):
    """Tests that we can generate a link to the revision list between the
    latest Chromium release and the last one we successfully built."""
    _TouchAndWrite(self.latest_new, stable_data)
    expected = cros_mark_chrome_as_stable.GetChromeRevisionLinkFromVersions(
        self.latest_stable_version, self.latest_new_version)
    made = cros_mark_chrome_as_stable.GetChromeRevisionListLink(
        cros_mark_chrome_as_stable.ChromeEBuild(self.latest_stable),
        cros_mark_chrome_as_stable.ChromeEBuild(self.latest_new),
        constants.CHROME_REV_LATEST)
    self.assertEqual(expected, made)

  def testTotChromeRevisionListLink(self):
    """Tests that we can generate a link to the revision list between ToT
    Chromium and the last ToT Chromium we successfully built."""
    _TouchAndWrite(self.tot_new,
                   '\n'.join(
                       (stable_data,
                        '%s=%s' % (cros_mark_chrome_as_stable._CHROME_SVN_TAG,
                                   new_fake_svn_rev))))

    expected = cros_mark_chrome_as_stable.GetChromeRevisionLinkFromRevisions(
        fake_svn_rev, new_fake_svn_rev)
    made = cros_mark_chrome_as_stable.GetChromeRevisionListLink(
        cros_mark_chrome_as_stable.ChromeEBuild(self.tot_stable),
        cros_mark_chrome_as_stable.ChromeEBuild(self.tot_new),
        constants.CHROME_REV_TOT)
    self.assertEqual(expected, made)

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
    self._CommonMarkAsStableTest(
        constants.CHROME_REV_STICKY,
        self.sticky_new_rc_version, self.sticky_rc,
        self.sticky_new_rc, 'stable_release')

  def testLatestMarkAsStable(self):
    """Tests to see if we can mark chrome for a latest release."""
    self._CommonMarkAsStableTest(
        constants.CHROME_REV_LATEST,
        self.latest_new_version, self.latest_stable,
        self.latest_new, 'latest_release')

  def testTotMarkAsStable(self):
    """Tests to see if we can mark chrome for tot."""
    self._CommonMarkAsStableTest(
        constants.CHROME_REV_TOT,
        self.tot_new_version, self.tot_stable,
        self.tot_new, 'tot')


if __name__ == '__main__':
  unittest.main()
