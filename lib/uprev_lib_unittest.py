# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""uprev_lib tests."""

from __future__ import print_function

import mock
import os

from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import uprev_lib
from chromite.lib.build_target_util import BuildTarget
from chromite.lib.chroot_lib import Chroot


class ChromeVersionTest(cros_test_lib.TestCase):
  """Tests for best_chrome_version and get_chrome_version_from_refs."""

  def setUp(self):
    self.best = '4.3.2.1'
    self.versions = ['1.2.3.4', self.best, '4.2.2.1', '4.3.1.4']
    self.best_ref = uprev_lib.GitRef('/path', self.best, 'abc123')
    self.refs = [uprev_lib.GitRef('/path', v, 'abc123') for v in self.versions]

    self.unstable = '9999'
    self.unstable_versions = self.versions + [self.unstable]

  def test_single_version(self):
    """Test a single version."""
    self.assertEqual(self.best, uprev_lib.best_chrome_version([self.best]))

  def test_multiple_versions(self):
    """Test a single version."""
    self.assertEqual(self.best, uprev_lib.best_chrome_version(self.versions))

  def test_no_versions_fail(self):
    """Test no versions given."""
    with self.assertRaises(AssertionError):
      uprev_lib.best_chrome_version([])

  def test_unstable_only(self):
    """Test the unstable version."""
    self.assertEqual(self.unstable,
                     uprev_lib.best_chrome_version([self.unstable]))

  def test_unstable_multiple(self):
    """Test unstable alongside multiple other versions."""
    self.assertEqual(self.unstable,
                     uprev_lib.best_chrome_version(self.unstable_versions))

  def test_single_ref(self):
    """Test a single ref."""
    self.assertEqual(self.best,
                     uprev_lib.get_chrome_version_from_refs([self.best_ref]))

  def test_multiple_refs(self):
    """Test multiple refs."""
    self.assertEqual(self.best,
                     uprev_lib.get_chrome_version_from_refs(self.refs))

  def test_no_refs_fail(self):
    """Test no versions given."""
    with self.assertRaises(AssertionError):
      uprev_lib.get_chrome_version_from_refs([])


class BestChromeEbuildTests(cros_test_lib.TempDirTestCase):
  """Test for best_chrome_ebuild."""

  def setUp(self):
    # Setup some ebuilds to test against.
    ebuild = os.path.join(self.tempdir, 'chromeos-chrome-%s-r12.ebuild')
    # The best version we'll be expecting.
    best_version = '4.3.2.1'
    best_ebuild_path = ebuild % best_version

    # Other versions to set up to compare against.
    versions = ['1.2.3.4', '4.3.2.0']
    ebuild_paths = [ebuild % version for version in versions]

    # Create a separate ebuild with the same chrome version as the best ebuild.
    rev_tiebreak_ebuild_path = best_ebuild_path.replace('-r12', '-r2')
    ebuild_paths.append(rev_tiebreak_ebuild_path)

    # Write stable ebuild data.
    stable_data = 'KEYWORDS=*'
    osutils.WriteFile(best_ebuild_path, stable_data)
    for path in ebuild_paths:
      osutils.WriteFile(path, stable_data)

    # Create the ebuilds.
    ebuilds = [uprev_lib.ChromeEBuild(path) for path in ebuild_paths]
    self.best_ebuild = uprev_lib.ChromeEBuild(best_ebuild_path)
    self.ebuilds = ebuilds + [self.best_ebuild]

  def test_no_ebuilds(self):
    """Test error on no ebuilds provided."""
    with self.assertRaises(AssertionError):
      uprev_lib.best_chrome_ebuild([])

  def test_single_ebuild(self):
    """Test a single ebuild."""
    best = uprev_lib.best_chrome_ebuild([self.best_ebuild])
    self.assertEqual(self.best_ebuild.ebuild_path, best.ebuild_path)

  def test_multiple_ebuilds(self):
    """Test multiple ebuilds."""
    best = uprev_lib.best_chrome_ebuild(self.ebuilds)
    self.assertEqual(self.best_ebuild.ebuild_path, best.ebuild_path)


class FindChromeEbuildsTest(cros_test_lib.TempDirTestCase):
  """find_chrome_ebuild tests."""

  def setUp(self):
    ebuild = os.path.join(self.tempdir, 'chromeos-chrome-%s.ebuild')
    self.unstable = ebuild % '9999'
    self.alpha_unstable = ebuild % '4.3.2.1_alpha-r12'
    self.best_stable = ebuild % '4.3.2.1_rc-r12'
    self.old_stable = ebuild % '4.3.2.1_rc-r2'

    unstable_data = 'KEYWORDS=~*'
    stable_data = 'KEYWORDS=*'

    osutils.WriteFile(self.unstable, unstable_data)
    osutils.WriteFile(self.alpha_unstable, unstable_data)
    osutils.WriteFile(self.best_stable, stable_data)
    osutils.WriteFile(self.old_stable, stable_data)

  def test_find_all(self):
    unstable, stables = uprev_lib.find_chrome_ebuilds(self.tempdir)
    self.assertEqual(self.unstable, unstable.ebuild_path)
    self.assertItemsEqual([self.best_stable, self.old_stable],
                          [stable.ebuild_path for stable in stables])


class UprevChromeManagerTest(cros_test_lib.MockTempDirTestCase):
  """UprevChromeManager tests."""

  def setUp(self):
    ebuild = 'chromeos-chrome-%s.ebuild'
    self.stable_chrome_version = '4.3.2.1'
    self.new_chrome_version = '4.3.2.2'
    self.stable_revision = 1
    stable_version = '%s_rc-r%d' % (self.stable_chrome_version,
                                    self.stable_revision)

    self.package_dir = os.path.join(self.tempdir, constants.CHROME_CP)
    osutils.SafeMakedirs(self.package_dir)

    self.stable_path = os.path.join(self.package_dir, ebuild % stable_version)
    self.unstable_path = os.path.join(self.package_dir, ebuild % '9999')

    osutils.WriteFile(self.stable_path, 'KEYWORDS=*')
    osutils.WriteFile(self.unstable_path, 'KEYWORDS=~*')

    # Avoid chroot interactions for the tests.
    self.PatchObject(uprev_lib, 'clean_stale_packages')

  def test_no_change(self):
    """Test a no-change uprev."""
    # No changes should be made when the stable and unstable ebuilds match.
    manager = uprev_lib.UprevChromeManager(
        self.stable_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    self.assertFalse(manager.modified_ebuilds)

  def test_new_version(self):
    """Test a new chrome version."""
    # The stable ebuild should be replaced with one of the new version.
    manager = uprev_lib.UprevChromeManager(
        self.new_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    # The old one should be deleted and the new one should exist.
    new_ebuild = self.stable_path.replace(self.stable_chrome_version,
                                          self.new_chrome_version)
    self.assertItemsEqual([self.stable_path, new_ebuild],
                          manager.modified_ebuilds)
    self.assertExists(new_ebuild)
    self.assertNotExists(self.stable_path)

  def test_uprev(self):
    """Test a revision bump."""
    # Make the contents different to force the uprev.
    osutils.WriteFile(self.unstable_path, 'IUSE=""', mode='a')
    manager = uprev_lib.UprevChromeManager(
        self.stable_chrome_version, overlay_dir=self.tempdir)
    manager.uprev(constants.CHROME_CP)

    new_ebuild = self.stable_path.replace('-r%d' % self.stable_revision,
                                          '-r%d' % (self.stable_revision + 1))

    self.assertItemsEqual([self.stable_path, new_ebuild],
                          manager.modified_ebuilds)
    self.assertExists(new_ebuild)
    self.assertNotExists(self.stable_path)


class UprevManagerTest(cros_test_lib.MockTestCase):
  """UprevManager tests."""

  def test_clean_stale_packages_no_chroot(self):
    """Test no chroot skip."""
    manager = uprev_lib.UprevOverlayManager([], None)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_not_called()

  def test_clean_stale_packages_chroot_not_exists(self):
    """Cannot run the commands when the chroot does not exist."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=False)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_not_called()

  def test_clean_stale_packages_no_build_targets(self):
    """Make sure it behaves as expected with no build targets provided."""
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([], None, chroot=chroot)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    # Make sure we aren't doing any work.
    patch.assert_called_once_with(mock.ANY, [[None]])

  def test_clean_stale_packages_with_boards(self):
    """Test it cleans all boards as well as the chroot."""
    targets = ['board1', 'board2']
    build_targets = [BuildTarget(t) for t in targets]
    chroot = Chroot()
    self.PatchObject(chroot, 'exists', return_value=True)
    manager = uprev_lib.UprevOverlayManager([],
                                            None,
                                            chroot=chroot,
                                            build_targets=build_targets)
    patch = self.PatchObject(parallel, 'RunTasksInProcessPool')

    # pylint: disable=protected-access
    manager._clean_stale_packages()

    patch.assert_called_once_with(mock.ANY, [[t] for t in targets + [None]])
