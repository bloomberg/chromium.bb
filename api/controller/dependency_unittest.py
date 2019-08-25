# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Board build dependency service tests."""

from __future__ import print_function

from chromite.api.controller import dependency
from chromite.api.gen.chromite.api import depgraph_pb2
from chromite.lib import cros_test_lib


class BoardBuilDependencyTest(cros_test_lib.TestCase):
  """Unittests for board_build_dependency."""

  def testCreateDepGraphProtoFromJsonMap(self):
    json_deps = {
        'target_board': 'deathstar',
        'package_deps': {
            'commander/darthvader-1.49.3.3': {
                'action': 'merge',
                'category': 'commander',
                'cpes': [],
                'deps': ['troop/clone', 'troop/robot'],
                'rev_deps': [],
                'full_name': 'commander/darthvader-1.49.3.3',
                'name': 'darthvader',
                'version': '1.49.3.3'
            },
            'troop/clone-1.2.3': {
                'action': 'merge',
                'category': 'troop',
                'cpes': [],
                'deps': [],
                'rev_deps': ['commander/darthvader'],
                'full_name': 'troop/clone-1.2.3',
                'name': 'clone',
                'version': '1.2.3'
            },
            'troop/robot-2.3.4': {
                'action': 'merge',
                'category': 'troop',
                'cpes': [],
                'deps': [],
                'rev_deps': ['commander/darthvader'],
                'full_name': 'troop/robot-2.3.4',
                'name': 'robot',
                'version': '2.3.4'
            },
        },
        'source_path_mapping': {
            'commander/darthvader-1.49.3.3': ['/control/room'],
            'troop/clone-1.2.3': ['/bunker'],
            'troop/robot-2.3.4': ['/factory'],
        },
    }

    depgraph_proto = depgraph_pb2.DepGraph()
    dependency.AugmentDepGraphProtoFromJsonMap(
        json_deps, depgraph_proto)
    self.assertEqual(depgraph_proto.build_target.name, 'deathstar')
    darthvader_dep = None
    for package_dep_info in depgraph_proto.package_deps:
      if package_dep_info.package_info.package_name == 'darthvader':
        darthvader_dep = package_dep_info
    self.assertTrue(darthvader_dep)
    self.assertEqual(darthvader_dep.dependency_packages[0].category,
                     'troop')
    self.assertEqual(darthvader_dep.dependency_packages[0].package_name,
                     'clone')
    self.assertEqual(darthvader_dep.dependency_source_paths[0].path,
                     '/control/room')
