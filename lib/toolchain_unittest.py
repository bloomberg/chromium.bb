# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for toolchain."""

from __future__ import print_function

import mock
import os

from chromite.lib import brick_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import toolchain
from chromite.lib import toolchain_list
from chromite.lib import workspace_lib


BASE_TOOLCHAIN_CONF = """# The root of all evil is money, err, this config.
base-target-name # This will become the base target.

# This toolchain is bonus!
bonus-toolchain {"a setting": "bonus value"}  # Bonus!

"""

ADDITIONAL_TOOLCHAIN_CONF = """# A helpful toolchain related comment.
extra-toolchain  # Unlikely to win any performance tests.

bonus-toolchain {"stable": true}
"""

MODERN_BSP_BRICK_CONFIG = {
    'name': 'bsp-brick',
    'toolchains': [('base-target-name', {'default': True}),
                   ('bonus-toolchain', {'a setting': 'bonus value'})
                  ],
    'dependencies': ['//custom-firmware-brick'],
}

MODERN_FIRMWARE_BRICK_CONFIG = {
    'name': 'custom-firmware-brick',
    'toolchains': [('bonus-toolchain', {'stable': True}),
                   ('extra-toolchain', {})],
}

TYPICAL_BRICK_WITHOUT_TOOLCHAINS = {
    'name': 'custom-firmware-brick',
}

EXPECTED_TOOLCHAINS = {
    'bonus-toolchain': {
        'sdk': True,
        'crossdev': '',
        'default': False,
        'a setting': 'bonus value',
        'stable': True,
    },
    'extra-toolchain': {'sdk': True, 'crossdev': '', 'default': False},
    'base-target-name': {'sdk': True, 'crossdev': '', 'default': True},
}


class ToolchainTest(cros_test_lib.MockTempDirTestCase):
  """Tests for lib.toolchain."""

  def _MakeBrick(self, config):
    return brick_lib.Brick(os.path.join(self.tempdir, config['name']),
                           initial_config=config)

  def setUp(self):
    self.PatchObject(workspace_lib, 'WorkspacePath', return_value=self.tempdir)

  def testArchForToolchain(self):
    """Tests that we correctly parse crossdev's output."""
    rc_mock = cros_build_lib_unittest.RunCommandMock()

    noarch = """target=foo
category=bla
"""
    rc_mock.SetDefaultCmdResult(output=noarch)
    with rc_mock:
      self.assertEqual(None, toolchain.GetArchForTarget('fake_target'))

    amd64arch = """arch=amd64
target=foo
"""
    rc_mock.SetDefaultCmdResult(output=amd64arch)
    with rc_mock:
      self.assertEqual('amd64', toolchain.GetArchForTarget('fake_target'))

  @mock.patch('chromite.lib.toolchain.portage_util.FindOverlays')
  def testReadsBoardToolchains(self, find_overlays_mock):
    """Tests that we correctly parse toolchain configs for an overlay stack."""
    # Create some fake overlays and put toolchain confs in a subset of them.
    overlays = [os.path.join(self.tempdir, 'overlay%d' % i) for i in range(3)]
    for overlay in overlays:
      osutils.SafeMakedirs(overlay)
    for overlay, contents in [(overlays[0], BASE_TOOLCHAIN_CONF),
                              (overlays[2], ADDITIONAL_TOOLCHAIN_CONF)]:
      osutils.WriteFile(os.path.join(overlay, 'toolchain.conf'), contents)
    find_overlays_mock.return_value = overlays
    actual_targets = toolchain.GetToolchainsForBoard('board_value')
    self.assertEqual(EXPECTED_TOOLCHAINS, actual_targets)

  def testReadsBrickToolchains(self):
    """Tests that we can read the toolchain for a brick stack."""
    # Creates the brick in a subdirectory of tempdir so that we can create other
    # bricks without interfering with it.
    self._MakeBrick(MODERN_FIRMWARE_BRICK_CONFIG)
    top_brick = self._MakeBrick(MODERN_BSP_BRICK_CONFIG)
    self.assertEqual(EXPECTED_TOOLCHAINS,
                     toolchain.GetToolchainsForBrick(top_brick.brick_locator))

  def testShouldDetectMissingDefaultsInBricks(self):
    """Tests that we check for a default toolchain in bricks."""
    brick = self._MakeBrick(
        {'name': 'brick-name', 'toolchains': [('base-toolchain', {})]})
    self.assertRaises(toolchain_list.NoDefaultToolchainDefinedError,
                      toolchain.GetToolchainsForBrick,
                      brick.brick_locator)

  def testShouldDetectConflictingOverrides(self):
    """Tests that we disallow toolchains with obvious conflicting settings."""
    conflicting_brick = self._MakeBrick(
        {'name': 'conflicting-brick',
         'toolchains': [
             ('base-toolchain', {'default': True,
                                 'setting': 'conflicting value'}),
         ],
        })
    brick = self._MakeBrick(
        {'name': 'bsp-brick',
         'toolchains': [
             ('base-toolchain', {'default': True,
                                 'setting': 'bsp value'}),
         ],
         'dependencies': [conflicting_brick.brick_locator],
        })
    self.assertRaises(toolchain_list.MismatchedToolchainConfigsError,
                      toolchain.GetToolchainsForBrick,
                      brick.brick_locator)

  def testToleratesBricksWithoutToolchains(self):
    """Tests that we correctly handle bricks that are toolchain agnostic."""
    simple_brick = self._MakeBrick(TYPICAL_BRICK_WITHOUT_TOOLCHAINS)
    toolchain.GetToolchainsForBrick(simple_brick.brick_locator)
