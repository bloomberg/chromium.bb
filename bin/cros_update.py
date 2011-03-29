#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Perform various tasks related to updating Portage packages."""

import glob
import logging
import optparse
import os
import parallel_emerge
import portage
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
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

  @staticmethod
  def _IsStableEBuild(ebuild_path):
    """Returns true if |ebuild_path| is a stable ebuild, false otherwise."""
    with open(ebuild_path, 'r') as ebuild:
      for line in ebuild:
        if line.startswith('KEYWORDS='):
          # TODO(petkov): Support non-x86 targets.
          return re.search(r'[ "]x86[ "]', line)
    return False

  def _SplitEBuildPath(self, ebuild_path):
    """Split a full ebuild path into (overlay, cat, pn, pv)."""
    logging.debug('ebuild: %s', ebuild_path)
    (ebuild_path, ebuild) = os.path.splitext(ebuild_path)
    (ebuild_path, pv) = os.path.split(ebuild_path)
    (ebuild_path, pn) = os.path.split(ebuild_path)
    (ebuild_path, cat) = os.path.split(ebuild_path)
    (ebuild_path, overlay) = os.path.split(ebuild_path)
    logging.debug('%s | %s | %s | %s', overlay, cat, pn, pv)
    return (overlay, cat, pn, pv)

  def _FindLatestVersion(self, upstream, cpv):
    """Returns the latest cpv in |upstream| if it's newer than |cpv|."""
    latest_cpv = cpv
    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    pkgpath = os.path.join(upstream, cat, pn)
    for ebuild_path in glob.glob(os.path.join(pkgpath, '%s*.ebuild' % pn)):
      if not Updater._IsStableEBuild(ebuild_path): continue
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      upstream_cpv = os.path.join(cat, pv)
      if portage.versions.pkgcmp(portage.versions.pkgsplit(upstream_cpv),
                                 portage.versions.pkgsplit(latest_cpv)) > 0:
        latest_cpv = upstream_cpv
    logging.debug('cpv: %s latest_cpv: %s', cpv, latest_cpv)
    if latest_cpv != cpv: return latest_cpv
    return None

  def _FindLatestVersions(self, cpvlist):
    """Given a list of cpvs, returns a list of cpv/latest_cpv info maps."""
    checkout_gentoo = False
    upstream = self._options.upstream
    if not upstream:
      checkout_gentoo = True
      upstream = os.path.join(self._options.srcroot, 'third_party', 'portage')
    infolist = []
    dash_q = ''
    if not self._options.verbose: dash_q = '-q'
    try:
      # TODO(petkov): Currently portage's master branch is stale so we need to
      # checkout latest upstream. At some point portage's master branch will be
      # upstream so there will be no need to chdir/checkout. At that point we
      # can also fuse this loop into the caller and avoid generating a separate
      # list.
      if checkout_gentoo:
        cros_lib.RunCommand(['/bin/sh', '-c',
                             'cd %s && git checkout %s cros/gentoo' % (
                                 upstream, dash_q)],
                            print_cmd=self._options.verbose);
      for cpv in cpvlist:
        # No need to report or try to upgrade chromeos-base packages.
        if cpv.startswith('chromeos-base/'): continue
        latest_cpv = self._FindLatestVersion(upstream, cpv)
        infolist.append({'cpv': cpv, 'latest_cpv': latest_cpv})
    finally:
      if checkout_gentoo:
        cros_lib.RunCommand(['/bin/sh', '-c',
                             'cd %s && git checkout %s cros/master' % (
                                 upstream, dash_q)],
                            print_cmd=self._options.verbose);
    return infolist

  def _PrintUpgrades(self):
    argv = ['--emptytree']
    argv.append('--board=%s' % self._options.board)
    if not self._options.verbose:
      argv.append('--quiet')
    if self._options.rdeps:
      argv.append('--root-deps=rdeps')
    argv.extend(self._args)

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)
    deps_tree, deps_info = deps.GenDependencyTree({})
    deps_graph = deps.GenDependencyGraph(deps_tree, deps_info, {})
    cpvlist = Updater._GetPreOrderDepGraph(deps_graph)

    infolist = self._FindLatestVersions(cpvlist)
    for info in infolist:
      # TODO(petkov): Use internal portage utilities to find the overlay instead
      # of equery to improve performance, if possible.
      cpv = info['cpv']
      equery = ['equery-%s' % self._options.board, 'which', cpv]
      ebuild_path = cros_lib.RunCommand(equery, print_cmd=self._options.verbose,
                                        redirect_stdout=True).output.strip()
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      info['overlay'] = overlay

    for info in infolist:
      update = ''
      if info['latest_cpv']: update = ' -> %s' % info['latest_cpv']
      print '[%s] %s%s' % (info['overlay'], info['cpv'], update)

  def Run(self):
    """Runs the updater based on the supplied options and arguments.

    Currently just lists all package dependencies in pre-order along with
    potential upgrades."""
    self._PrintUpgrades()


def main():
  usage = 'Usage: %prog [options] packages...'
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('--board', dest='board', type='string', action='store',
                    default=None, help="Target board [default: '%default']")
  parser.add_option('--rdeps', dest='rdeps', action='store_true',
                    default=False,
                    help="Use runtime dependencies only")
  parser.add_option('--srcroot', dest='srcroot', type='string', action='store',
                    default='%s/trunk/src' % os.environ['HOME'],
                    help="Path to root src directory [default: '%default']")
  parser.add_option('--upstream', dest='upstream', type='string',
                    action='store', default=None,
                    help="Latest upstream repo location [default: '%default']")
  parser.add_option('--verbose', dest='verbose', action='store_true',
                    default=False,
                    help="Enable verbose output (for debugging)")

  (options, args) = parser.parse_args()

  if (options.verbose): logging.basicConfig(level=logging.DEBUG)

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
