# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build graph dependency creation service.

This service handles the creation of the portage build dependency graphs and the
graphs mapping from portage packages to the dependency source.
"""

from __future__ import print_function

from chromite.api import faux
from chromite.api import validate
from chromite.lib import portage_util
from chromite.service import dependency


def AugmentDepGraphProtoFromJsonMap(json_map, graph):
  """Augment package deps from |json_map| to graph object.

  Args:
    json_map: the json object that stores the portage package. This is
      generated from chromite.lib.service.dependency.GetBuildDependency()
    graph: the proto object that represents the dependency graph (see DepGraph
      message in chromite/api/depgraph.proto)
  """
  graph.build_target.name = json_map['target_board']

  for data in json_map['package_deps'].values():
    package_dep_info = graph.package_deps.add()
    package_info = package_dep_info.package_info
    package_info.package_name = data['name']
    package_info.category = data['category']
    package_info.version = data['version']
    for dep in data['deps']:
      cpv = portage_util.SplitCPV(dep, strict=False)
      dep_package = package_dep_info.dependency_packages.add()
      dep_package.package_name = cpv.package
      dep_package.category = cpv.category
      if cpv.version:
        dep_package.version = cpv.version

    package_CPV = '%s/%s-%s' % (
        package_info.category, package_info.package_name, package_info.version)
    for path in json_map['source_path_mapping'][package_CPV]:
      source_path = package_dep_info.dependency_source_paths.add()
      source_path.path = path


@faux.all_empty
@validate.require('build_target.name')
@validate.validation_complete
def GetBuildDependencyGraph(input_proto, output_proto, _config):
  """Create the build dependency graph.

  Args:
    input_proto (GetBuildDependencyGraphRequest): The input arguments message.
    output_proto (GetBuildDependencyGraphResponse): The empty output message.
    _config (api_config.ApiConfig): The API call config.
  """
  board = input_proto.build_target.name

  json_map = dependency.GetBuildDependency(board)
  AugmentDepGraphProtoFromJsonMap(json_map, output_proto.dep_graph)
