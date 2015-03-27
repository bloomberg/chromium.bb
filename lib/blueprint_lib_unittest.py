# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the blueprint library."""

from __future__ import print_function

import json
import os

from chromite.lib import blueprint_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import workspace_lib


class BlueprintLibTest(cros_test_lib.MockTempDirTestCase):
  """Unittest for blueprint_lib.py"""

  def setUp(self):
    self.blueprint = None
    self.SetupFakeWorkspace()

  def SetupBlueprint(self, blueprint_name='foo.json', bricks=None, bsp=None,
                     main_package=None):
    path = os.path.join(self.tempdir, blueprint_name)

    config = {}
    if bricks:
      config['bricks'] = bricks
    if bsp:
      config['bsp'] = bsp
    if main_package:
      config['main_package'] = main_package

    osutils.WriteFile(path, json.dumps(config, sort_keys=True, indent=4,
                                       separators=(',', ': ')),
                      makedirs=True)

    self.blueprint = blueprint_lib.Blueprint(path)

  def SetupFakeWorkspace(self):
    self.PatchObject(workspace_lib, 'WorkspacePath', return_value=self.tempdir)

  def testBlueprint(self):
    bricks = ['//foo', '//bar', '//baz']
    self.SetupBlueprint(bricks=bricks, bsp='//bsp',
                        main_package='virtual/target-os')
    self.assertEqual(self.blueprint.GetBricks(), bricks)
    self.assertEqual(self.blueprint.GetBSP(), '//bsp')
    self.assertEqual(self.blueprint.GetMainPackage(), 'virtual/target-os')

  def testBlueprintNoBricks(self):
    self.SetupBlueprint(bsp='//bsp2', main_package='virtual/target-os-dev')
    self.assertEqual(self.blueprint.GetBricks(), [])
    self.assertEqual(self.blueprint.GetBSP(), '//bsp2')
    self.assertEqual(self.blueprint.GetMainPackage(), 'virtual/target-os-dev')
