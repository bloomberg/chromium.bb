# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build graph dependency creation service.

This service handles the creation of the portage build dependency graphs and the
graphs mapping from portage packages to the dependency source.
"""

from __future__ import print_function

import json

from chromite.lib.api import dependency


def GetBuildDependencyGraph(input_proto, output_proto):
  """Create the build dependency graph.

  Args:
    input_proto (GetBuildDependencyGraphRequest): The input arguments message.
    output_proto (GetBuildDependencyGraphResponse): The empty output message.
  """
  board = input_proto.build_target.name
  output_path = input_proto.output_path

  assert board, 'Missing build target name'
  assert output_path, 'Missing output file'

  json_map = dependency.GetBuildDependency(board)
  with open(output_path, 'w') as f:
    json.dump(json_map, f, indent=2, sort_keys=True)

  output_proto.build_dependency_graph_file = output_path
