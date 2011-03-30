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
import shutil
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib


class Upgrader(object):
  """A class to perform various tasks related to updating Portage packages."""

  def __init__(self, options, args):
    self._options = options
    self._args = args
    self._stable_repo = os.path.join(self._options.srcroot,
                                     'third_party', 'portage-stable')
    self._upstream_repo = self._options.upstream
    if not self._upstream_repo:
      self._upstream_repo = os.path.join(self._options.srcroot,
                                         'third_party', 'portage')

  @staticmethod
  def _GetPreOrderDepGraphPackage(deps_graph, package, pkglist, visited):
    """Collect packages from |deps_graph| into |pkglist| in pre-order."""
    if package in visited:
      return
    visited.add(package)
    for parent in deps_graph[package]['provides']:
      Upgrader._GetPreOrderDepGraphPackage(deps_graph, parent, pkglist, visited)
    pkglist.append(package)

  @staticmethod
  def _GetPreOrderDepGraph(deps_graph):
    """Return packages from |deps_graph| in pre-order."""
    pkglist = []
    visited = set()
    for package in deps_graph:
      Upgrader._GetPreOrderDepGraphPackage(deps_graph, package, pkglist,
                                           visited)
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

  def _RunGit(self, repo, command):
    """Runs |command| in the git |repo|."""
    cros_lib.RunCommand(['/bin/sh', '-c', 'cd %s && git %s' % (repo, command)],
                        print_cmd=self._options.verbose);

  def _SplitEBuildPath(self, ebuild_path):
    """Split a full ebuild path into (overlay, cat, pn, pv)."""
    (ebuild_path, ebuild) = os.path.splitext(ebuild_path)
    (ebuild_path, pv) = os.path.split(ebuild_path)
    (ebuild_path, pn) = os.path.split(ebuild_path)
    (ebuild_path, cat) = os.path.split(ebuild_path)
    (ebuild_path, overlay) = os.path.split(ebuild_path)
    return (overlay, cat, pn, pv)

  def _FindLatestVersion(self, cpv):
    """Returns the latest cpv in |_upstream_repo| if it's newer than |cpv|."""
    latest_cpv = cpv
    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    pkgpath = os.path.join(self._upstream_repo, cat, pn)
    for ebuild_path in glob.glob(os.path.join(pkgpath, '%s*.ebuild' % pn)):
      if not Upgrader._IsStableEBuild(ebuild_path): continue
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      upstream_cpv = os.path.join(cat, pv)
      if portage.versions.pkgcmp(portage.versions.pkgsplit(upstream_cpv),
                                 portage.versions.pkgsplit(latest_cpv)) > 0:
        latest_cpv = upstream_cpv
    if latest_cpv != cpv: return latest_cpv
    return None

  def _CopyUpstreamPackage(self, info):
    """Upgrades package described by |info| by copying from usptream.

    The copy is performed only if it's possible to upgrade the package and
    --upgrade is specified.

    Returns:
      True if the packages was upgraded, False otherwise.
    """
    latest_cpv = info['latest_cpv']
    if (not latest_cpv or not self._options.upgrade or
        not info['overlay'].startswith('portage')):
      return False
    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(latest_cpv)
    catpkgname = os.path.join(cat, pkgname)
    pkgdir = os.path.join(self._stable_repo, catpkgname)
    if os.path.exists(pkgdir):
      shutil.rmtree(pkgdir)
    upstream_pkgdir = os.path.join(self._upstream_repo, cat, pkgname)
    # Copy the whole package except the ebuilds.
    shutil.copytree(upstream_pkgdir, pkgdir,
                    ignore=shutil.ignore_patterns('*.ebuild'))
    # Copy just the ebuild that will be used in the build.
    shutil.copy2(os.path.join(upstream_pkgdir,
                              latest_cpv.split('/')[1] + '.ebuild'), pkgdir)
    self._RunGit(self._stable_repo, 'add ' + catpkgname)
    return True

  def _UpgradePackage(self, info):
    """Updates |info| with latest_cpv and performs an upgrade if necessary."""
    cpv = info['cpv']
    # No need to report or try to upgrade chromeos-base packages.
    if cpv.startswith('chromeos-base/'): return
    info['latest_cpv'] = self._FindLatestVersion(cpv)
    info['upgraded'] = self._CopyUpstreamPackage(info)
    upgrade = ''
    if info['latest_cpv']: upgrade = ' -> %s' % info['latest_cpv']
    upgraded = ''
    if info['upgraded']: upgraded = ' (UPGRADED)'
    print '[%s] %s%s%s' % (info['overlay'], info['cpv'], upgrade, upgraded)

  def _UpgradePackages(self, infolist):
    """Given a list of cpv info maps, adds the latest_cpv to the infos."""
    dash_q = ''
    if not self._options.verbose: dash_q = '-q'
    try:
      # TODO(petkov): Currently portage's master branch is stale so we need to
      # checkout latest upstream. At some point portage's master branch will be
      # upstream so there will be no need to chdir/checkout. At that point we
      # can also fuse this loop into the caller and avoid generating a separate
      # list.
      if not self._options.upstream:
        self._RunGit(self._upstream_repo, 'checkout %s cros/gentoo' % dash_q)
      message = ''
      for info in infolist:
        self._UpgradePackage(info)
        if info['upgraded']:
          message += 'Upgrade %s to %s\n' % (info['cpv'], info['latest_cpv'])
      if message:
        message = 'Upgrade Portage packages\n\n' + message
        message += '\nBUG=<fill-in>'
        message += '\nTEST=<fill-in>'
        self._RunGit(self._stable_repo, "commit -am '%s'" % message)
        cros_lib.Info('Use "git commit --amend" to update the commit message.')
    finally:
      if not self._options.upstream:
        self._RunGit(self._upstream_repo, 'checkout %s cros/master' % dash_q)

  def _GetCurrentVersions(self):
    """Returns a list of cpvs of the current package dependencies."""
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
    return Upgrader._GetPreOrderDepGraph(deps_graph)

  def _GetInfoListWithOverlays(self, cpvlist):
    """Returns a list of cpv/overlay info maps corresponding to |cpvlist|."""
    infolist = []
    for cpv in cpvlist:
      # TODO(petkov): Use internal portage utilities to find the overlay instead
      # of equery to improve performance, if possible.
      equery = ['equery-%s' % self._options.board, 'which', cpv]
      ebuild_path = cros_lib.RunCommand(equery, print_cmd=self._options.verbose,
                                        redirect_stdout=True).output.strip()
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      infolist.append({'cpv': cpv, 'overlay': overlay})

    return infolist

  def Run(self):
    """Runs the upgrader based on the supplied options and arguments.

    Currently just lists all package dependencies in pre-order along with
    potential upgrades."""
    cpvlist = self._GetCurrentVersions()
    infolist = self._GetInfoListWithOverlays(cpvlist)
    self._UpgradePackages(infolist)

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
  parser.add_option('--upgrade', dest='upgrade', action='store_true',
                    default=False,
                    help="Perform package upgrade")
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

  upgrader = Upgrader(options, args)
  upgrader.Run()


if __name__ == '__main__':
  main()
