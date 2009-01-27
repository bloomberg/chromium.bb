#!/usr/bin/python

import os.path
import sys
import simplejson as json
import xcodeproj

def BuildFileAndTarget(build_file, target):
  # NOTE: If you just want to split up target into a build_file and target,
  # and you know that target already has a build_file that's been produced by
  # this function, pass "" for build_file.

  target_split = target.split(":", 1)
  if len(target_split) == 2:
    [build_file_rel, target] = target_split

    # If a relative path, build_file_rel is relative to the directory
    # containing build_file.  If build_file is not in the current directory,
    # build_file_rel is not a usable path as-is.  Resolve it by interpreting it
    # as relative to build_file.  If build_file_rel is absolute, it is usable
    # as a path regardless of the current directory, and os.path.join will
    # return it as-is.
    build_file = os.path.join(os.path.dirname(build_file), build_file_rel)

  return [build_file, target, build_file + ":" + target]

def QualifiedTarget(build_file, target):
  return BuildFileAndTarget(build_file, target)[2]

def ExceptionAppend(e, msg):
  if not e.args:
    e.args = [msg]
  else:
    if len(e.args) == 1:
      e.args = [e.args[0] + " " + msg]
    else:
      e.args = [e.args[0] + " " + msg, e.args[1:]]

def ReadBuildFile(build_file_path, data={}):
  build_file = open(build_file_path)
  try:
    build_file_data = json.load(build_file)
    data[build_file_path] = build_file_data
  except Exception, e:
    ExceptionAppend(e, "while reading " + build_file_path)
    raise
  finally:
    build_file.close()

  # Look for references to other build files that may need to be read.
  if "targets" in build_file_data:
    for target_name, target_dict in build_file_data["targets"].iteritems():
      if "dependencies" in target_dict:
        for dependency in target_dict["dependencies"]:
          other_build_file = BuildFileAndTarget(build_file_path, dependency)[0]
          if not other_build_file in data:
            ReadBuildFile(other_build_file, data)

  return data

def MakeXcodeProj(targets, data):
  xcode_projects = {}
  for build_file, build_file_dict in data.iteritems():
    if build_file[-6:] != ".build":
      # TODO(mark): Pick an exception class
      raise "Build file name must end in .build"
    build_file_stem = build_file[:-6]
    xcode_projects[build_file] = xcodeproj.XcodeProject(build_file_stem)

  xcode_targets = {}
  for qualified_target in targets:
    [build_file, target] = BuildFileAndTarget("", qualified_target)[0:2]
    spec = data[build_file]["targets"][target]
    xcode_targets[qualified_target] = \
        xcode_projects[build_file].AddTarget(target, spec["type"])
    for source in spec["sources"]:
      xcode_targets[qualified_target].SourcesPhase().AddFile(source)
    if "dependencies" in spec:
      for dependency in spec["dependencies"]:
        dependency = QualifiedTarget(build_file, dependency)
        xcode_targets[qualified_target].AddDependency(xcode_targets[dependency])

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
        target_build_file = BuildFileAndTarget("", target)[0]
        dependency = QualifiedTarget(target_build_file, dependency)
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
      qualified_target = QualifiedTarget(build_file_name, target)
      targets[qualified_target] = build_file_data["targets"][target]

  flat_list = BuildDependencyList(targets)

  MakeXcodeProj(flat_list, data)

if __name__ == "__main__":
  main()
