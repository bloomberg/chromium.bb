#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Perform various tasks related to updating Portage packages."""

import optparse
import os
import parallel_emerge
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
import chromite.lib.cros_build_lib as cros_lib


class Updater(object):
  """A class to perform various tasks related to updating Portage packages."""

  def __init__(self, options, args):
    self._options = options
    self._args = args

  @staticmethod
  def _GetPreOrderDepGraphPackage(deps_graph, package, pkglist, visited):
    """Collect packages from |deps_graph| into |pkglist| in pre-order."""
    if package in visited:
      return
    visited.add(package)
    for parent in deps_graph[package]['provides']:
      Updater._GetPreOrderDepGraphPackage(deps_graph, parent, pkglist, visited)
    pkglist.append(package)

  @staticmethod
  def _GetPreOrderDepGraph(deps_graph):
    """Return packages from |deps_graph| in pre-order."""
    pkglist = []
    visited = set()
    for package in deps_graph:
      Updater._GetPreOrderDepGraphPackage(deps_graph, package, pkglist, visited)
    return pkglist

  def _PrintDependencies(self):
    argv = ['--quiet', '--emptytree']
    argv.append('--board=%s' % self._options.board)
    if self._options.rdeps:
      argv.append('--root-deps=rdeps')
    argv.extend(self._args)

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)
    deps_tree, deps_info = deps.GenDependencyTree({})
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info, {})
    for package in Updater._GetPreOrderDepGraph(deps_graph):
      if self._options.full_path:
        equery = ['equery-%s' % self._options.board, 'which', package]
        package = cros_lib.RunCommand(equery, print_cmd=False,
                                      redirect_stdout=True).output.strip()
      print package

  def Run(self):
    """Runs the updater based on the supplied options and arguments.

    Currently just lists all package dependencies in pre-order."""
    self._PrintDependencies()


def main():
  usage = 'Usage: %prog [options] packages...'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--board', dest='board', type='string', action='store',
                    default=None, help="Target board [default: '%default']")
  parser.add_option('--no-full-path', dest='full_path', action='store_false',
                    default=True,
                    help="Don't print the full path to the package")
  parser.add_option('--rdeps', dest='rdeps', action='store_true',
                    default=False,
                    help="Use runtime dependencies only")

  (options, args) = parser.parse_args()

  if not options.board:
    parser.print_help()
    cros_lib.Die('board is required')

  if not args:
    parser.print_help()
    cros_lib.Die('no packages provided')

  updater = Updater(options, args)
  updater.Run()


if __name__ == '__main__':
  main()
