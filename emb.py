#!/usr/bin/python

import os.path
import sys
import simplejson as json
import xcodeproj

def ReadBuildFile(build_file_path, build_file_data={}):
  build_file = open(build_file_path)
  try:
    build_file_data[build_file_path] = json.load(build_file)
  finally:
    build_file.close()

  # Look for references to other build files that may need to be read.
  if "targets" in build_file_data[build_file_path]:
    for target_name, target_dict in build_file_data[build_file_path]["targets"].iteritems():
      if "dependencies" in target_dict:
        # TODO(mark): Some of this stuff should be refactored into separate
        # functions.
        for dependency in target_dict["dependencies"]:
          dep_split = dependency.split(":", 1)
          if len(dep_split) == 2:
            other_build_file = \
                os.path.join(os.path.dirname(build_file_path), dep_split[0])
            if not other_build_file in build_file_data:
              ReadBuildFile(other_build_file, build_file_data)

  return build_file_data

def MakeXcodeProj(targets, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-6:] != ".build":
      # TODO(mark): Pick an exception class
      raise "Build file name must end in .build"
    build_file_stem = build_file[:-6]
    xcode_projects[build_file] = xcodeproj.XcodeProject(build_file_stem)

  xcode_targets = {}
  for bf_and_target in targets:
    [build_file, target] = bf_and_target.split(":", 1)
    spec = data[build_file]["targets"][target]
    xcode_targets[bf_and_target] = xcode_projects[build_file].AddTarget(target, spec["type"])
    for source in spec["sources"]:
      xcode_targets[bf_and_target].SourcesPhase().AddFile(source)
    if "dependencies" in spec:
      for dependency in spec["dependencies"]:
        dep_split = dependency.split(":", 1)
        if len(dep_split) == 1:
          dependency = build_file + ":" + dependency
        xcode_targets[bf_and_target].AddDependency(xcode_targets[dependency])

  for build_file, build_file_dict in data.iteritems():
    xcode_projects[build_file].Write()

class DependencyTreeNode(object):
  """

  Class variables:
     MARK_NONE: An object that has not yet been visited during flattening.
     MARK_PENDING: An object that has been visited, but whose dependencies
                   have not yet been visisted.
     MARK_DONE: An object that has been visited and whose dependencies have
                also been visited.
  Attributes:
     ref: A reference to an object that this DependencyTreeNode represents.
     dependencies: List of DependencyTreeNodes on which this one depends.
     dependents: List of DependencyTreeNodes that depend on this one.
     _mark: That's me.  Also, this is set to a MARK_* constant to the mark
            state of the object during flattening.
  """

  MARK_NONE = 0
  MARK_PENDING = 1
  MARK_DONE = 2

  class CircularException(Exception):
    pass

  def __init__(self, ref):
    self.ref = ref
    self.dependencies = []
    self.dependents = []
    self._mark = self.MARK_NONE

  def FlattenToList(self, flat_list=[]):
    if self._mark == self.MARK_PENDING:
      # Oops.  Found a cycle.
      raise self.CircularException, "Returned to " + str(self.ref) + \
                                    " while it was already being visited"

    if self._mark == self.MARK_DONE:
      # Already visisted.
      return

    # Visit all nodes upon which this one depends, or ensure that they have
    # already been visited.
    self._mark = self.MARK_PENDING
    for dependency in self.dependencies:
      dependency.FlattenToList(flat_list)

    # All of this node's dependency references are in the list.  Append this
    # node's reference.
    self._mark = self.MARK_DONE
    flat_list.append(self.ref)

    # Visit all nodes that depend on this one.
    for dependent in self.dependents:
      dependent.FlattenToList(flat_list)

    return flat_list


def BuildDependencyList(targets):
  # Create a DependencyTreeNode for each target.  Put it into a dict for easy
  # access.
  dependency_nodes = {}
  for target, spec in targets.iteritems():
    if not target in dependency_nodes:
      dependency_nodes[target] = DependencyTreeNode(target)

  # Set up the dependency links.  Targets that have no dependencies are treated
  # as dependent on root_node.
  root_node = DependencyTreeNode(None)
  for target, spec in targets.iteritems():
    target_node = dependency_nodes[target]
    if not "dependencies" in spec or len(spec["dependencies"]) == 0:
      target_node.dependencies = [root_node]
      root_node.dependents.append(target_node)
    else:
      for dependency in spec["dependencies"]:
        dep_split = dependency.split(":", 1)
        if len(dep_split) == 1:
          targ_split = target.split(":", 1)
          dependency = targ_split[0] + ":" + dependency
        dependency_node = dependency_nodes[dependency]
        target_node.dependencies.append(dependency_node)
        dependency_node.dependents.append(target_node)

  # Take the root node out of the list because it doesn't correspond to a real
  # target.
  flat_list = root_node.FlattenToList()[1:]

  # If there's anything left unvisited, there must be a self-contained circular
  # dependency.  If you need to figure out what's wrong, look for elements of
  # targets that are not in flat_list.  FlattenToList doesn't catch these
  # self-contained cycles because they're not reachable from root_node.
  if len(flat_list) != len(targets):
    raise DependencyTreeNode.CircularException, "some targets not reachable"

  return flat_list


def main():
  build_file = sys.argv[1]
  data = ReadBuildFile(build_file)

  targets = {}
  for build_file_name, build_file_data in data.iteritems():
    for target in build_file_data["targets"]:
      targets[build_file_name + ":" + target] = build_file_data["targets"][target]

  flat_list = BuildDependencyList(targets)

  MakeXcodeProj(flat_list, data)

if __name__ == "__main__":
  main()
