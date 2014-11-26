#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros bootstrap-overlay command."""

from __future__ import print_function

import os
import re
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))

from chromite.cros.commands import cros_bootstrap_overlay
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils

class BootstrapOverlayCommandTest(cros_test_lib.TempDirTestCase):
  """Test class for the bootstrap-overlay command."""

  def _createSimpleOverlay(self, destination):
    """Creates a test overlay called fakeoverlay.

    This creates:
      * the virtual bsp
      * the bsp implementation (in chromeos-base)
      * the layout.conf file
      * the legacy repo_name file
    """
    bsp_virtual_path = os.path.join(destination, 'virtual', 'chromeos-bsp',
                                    'chromeos-bsp-2.ebuild')
    bsp_virtual_content = 'RDEPEND="chromeos-base/chromeos-bsp-fakeoverlay"'
    osutils.WriteFile(bsp_virtual_path, bsp_virtual_content, makedirs=True)

    bsp_impl_path = os.path.join(destination, 'chromeos-base',
                                 'chromeos-bsp-fakeoverlay',
                                 'chromeos-bsp-fakeoverlay-0.0.1.ebuild')
    bsp_impl_content = \
"""
RDEPEND="
	chromeos-base/something
	dev-cpp/gtest
"

DEPEND="chromeos-base/otherthing"
"""
    osutils.WriteFile(bsp_impl_path, bsp_impl_content, makedirs=True)

    osutils.WriteFile(os.path.join(destination, 'profiles', 'repo_name'),
                      "fakeoverlay",
                      makedirs=True)

    osutils.WriteFile(os.path.join(destination, 'metadata', 'layout.conf'),
                      'masters = gentoo\nfield = value',
                      makedirs=True)

  def _getAtomList(self, ebuild_path, variable="RDEPEND"):
    """Get the list atom from the DEPEND/RDEPEND variable of an ebuild.

    Args:
      ebuild_path: path to the ebuild.
      variable: variable to parse

    Returns:
      The list of atoms stored in the variable, stripped of any whitespace.
      This does not handle incremental variable building.
    """
    content = osutils.ReadFile(ebuild_path)
    match = re.search('%s="([^"]+)"' % variable, content)
    if match:
      atom_list = match.group(1).split('\n')
      return [a.strip() for a in atom_list]
    return []

  def testCreateVirtual(self):
    """Test that the virtuals are created correctly.

    The right atoms should be included in the virtual bsp and the bsp
    implementation.
    """
    APP_ATOM = 'app-misc/appname'

    self._createSimpleOverlay(self.tempdir)
    osutils.RmDir(os.path.join(self.tempdir, 'virtual'))
    osutils.RmDir(os.path.join(self.tempdir, 'chromeos-base'))

    cros_bootstrap_overlay.CreateBsp('boardname',
                                     self.tempdir,
                                     APP_ATOM)

    virtual_atomlist = self._getAtomList(os.path.join(self.tempdir,
                                                      'virtual',
                                                      'chromeos-bsp',
                                                      'chromeos-bsp-2.ebuild'))
    self.assertTrue('chromeos-base/chromeos-bsp-boardname' in virtual_atomlist)

    bsp_file = os.path.join(self.tempdir,
                            'chromeos-base',
                            'chromeos-bsp-boardname',
                            'chromeos-bsp-boardname-0.0.1.ebuild')

    self.assertTrue(APP_ATOM in self._getAtomList(bsp_file))

  def testRepoNameRemove(self):
    """Tests that repo_name is removed."""
    self._createSimpleOverlay(self.tempdir)

    cros_bootstrap_overlay.RemoveLegacyRepoName(self.tempdir)
    self.assertFalse(os.path.isfile(os.path.join(self.tempdir,
                                                 'profiles',
                                                 'repo_name')))

  def testLayoutUpdate(self):
    """Tests that the layout is changed to the right repo-name."""
    self._createSimpleOverlay(self.tempdir)

    cros_bootstrap_overlay.UpdateLayout(self.tempdir, 'hello')

    dic = cros_build_lib.LoadKeyValueFile(os.path.join(self.tempdir,
                                                       'metadata',
                                                       'layout.conf'))
    self.assertEquals(dic['repo-name'], 'hello')


if __name__ == '__main__':
  cros_test_lib.main()
