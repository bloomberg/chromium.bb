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
import chromite.lib.table as table

class UpgradeTable(table.Table):
  """Class to represent upgrade data in memory, can be written to csv/html."""

  # These constants serve two roles, for both csv and html table output:
  # 1) Restrict which column names are valid.
  # 2) Specify the order of those columns.
  COL_PACKAGE = 'Package'
  COL_OVERLAY = 'Overlay'
  COL_CURRENT_VER = 'Current Version'
  COL_UPSTREAM_VER = 'Stable Upstream Version'
  COL_STATE = 'State'
  COL_DEPENDS_ON = 'Depends On'
  COL_ACTION_TAKEN = 'Action Taken'
  COLUMNS = [COL_PACKAGE,
             COL_OVERLAY,
             COL_CURRENT_VER,
             COL_UPSTREAM_VER,
             COL_STATE,
             COL_DEPENDS_ON,
             COL_ACTION_TAKEN,
             ]

  # COL_STATE values should be one of the following
  STATE_UNKNOWN = 'unknown'
  STATE_NEEDS_UPGRADE = 'needs upgrade'
  STATE_PATCHED = 'patched locally'
  STATE_CURRENT = 'current'

  # COL_ACTION_TAKEN values should be one of the following
  ACTION_UPGRADED = 'upgraded'
  ACTION_NONE = ''

  def __init__(self):
    table.Table.__init__(self, UpgradeTable.COLUMNS)

  def WriteHTML(self, filehandle):
    """Write table out as a custom html table to |filehandle|."""
    # Basic HTML, up to and including start of table and table headers.
    filehandle.write('<html>\n')
    filehandle.write('  <table border="1" cellspacing="0" cellpadding="3">\n')
    filehandle.write('    <caption>Portage Package Status</caption>\n')
    filehandle.write('    <thead>\n')
    filehandle.write('      <tr>\n')
    filehandle.write('        <th>%s</th>\n' %
             '</th>\n        <th>'.join(self._columns))
    filehandle.write('      </tr>\n')
    filehandle.write('    </thead>\n')
    filehandle.write('    <tbody>\n')

    # Now write out the rows.
    for row in self._rows:
      filehandle.write('      <tr>\n')
      for col in self._columns:
        val = row.get(col, "")

        # Add color to the text in specific cases.
        if val and col == self.COL_STATE:
          # Add colors for state column.
          if val == self.STATE_NEEDS_UPGRADE or val == self.STATE_UNKNOWN:
            val = '<span style="color:red">%s</span>' % val
          elif val == self.STATE_CURRENT:
            val = '<span style="color:green">%s</span>' % val
        if val and col == self.COL_DEPENDS_ON:
          # Add colors for dependencies column.  If a dependency is itself
          # out of date, then make it red.
          vallist = []
          for cpv in val.split(' '):
            # Get category/packagename from cpv, in order to look up row for
            # the dependency.  Then see if that pkg is red in its own row.
            catpkg = Upgrader._GetCatPkgFromCpv(cpv)
            deprow = self.GetRowsByValue({self.COL_PACKAGE: catpkg})[0]
            if (deprow[self.COL_STATE] == self.STATE_NEEDS_UPGRADE or
                deprow[self.COL_STATE] == self.STATE_UNKNOWN):
              vallist.append('<span style="color:red">%s</span>' % cpv)
            else:
              vallist.append(cpv)
          val = ' '.join(vallist)

        filehandle.write('        <td>%s</td>\n' % val)

      filehandle.write('      </tr>\n')

    # Finish the table and html
    filehandle.write('    </tbody>\n')
    filehandle.write('  </table>\n')
    filehandle.write('</html>\n')

class Upgrader(object):
  """A class to perform various tasks related to updating Portage packages."""

  def __init__(self, options=None, args=None):
    # TODO(mtennant): Don't save options object, but instead grab option
    # values that are needed now.  This makes mocking easier.
    # For example: self._board = options.board
    self._options = options
    self._args = args
    self._stable_repo = os.path.join(self._options.srcroot,
                                     'third_party', 'portage-stable')
    self._upstream_repo = self._options.upstream
    if not self._upstream_repo:
      self._upstream_repo = os.path.join(self._options.srcroot,
                                         'third_party', 'portage')
    self._table = UpgradeTable()

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

  @staticmethod
  def _CmpCpv(cpv1, cpv2):
    """Returns standard cmp result between |cpv1| and |cpv2|."""
    return portage.versions.pkgcmp(portage.versions.pkgsplit(cpv1),
                                   portage.versions.pkgsplit(cpv2))

  @staticmethod
  def _GetVerRevFromCpv(cpv):
    """Returns just the version-revision string from a full |cpv|."""
    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    return '%s-%s' % (version, rev)

  @staticmethod
  def _GetCatPkgFromCpv(cpv):
    """Returns just the category/packagename string from a full |cpv|."""
    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    return '%s/%s' % (cat, pn)

  def _RunGit(self, repo, command):
    """Runs |command| in the git |repo|."""
    cros_lib.RunCommand(['/bin/sh', '-c', 'cd %s && git %s' % (repo, command)],
                        print_cmd=self._options.verbose)

  def _SplitEBuildPath(self, ebuild_path):
    """Split a full ebuild path into (overlay, cat, pn, pv)."""
    (ebuild_path, ebuild) = os.path.splitext(ebuild_path)
    (ebuild_path, pv) = os.path.split(ebuild_path)
    (ebuild_path, pn) = os.path.split(ebuild_path)
    (ebuild_path, cat) = os.path.split(ebuild_path)
    (ebuild_path, overlay) = os.path.split(ebuild_path)
    return (overlay, cat, pn, pv)

  def _FindLatestVersion(self, cpv):
    """Returns latest cpv in |_upstream_repo| with same cat/pkg as |cpv|."""
    latest_cpv = None
    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    pkgpath = os.path.join(self._upstream_repo, cat, pn)
    for ebuild_path in glob.glob(os.path.join(pkgpath, '%s*.ebuild' % pn)):
      if not Upgrader._IsStableEBuild(ebuild_path): continue

      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      upstream_cpv = os.path.join(cat, pv)
      if (not latest_cpv) or Upgrader._CmpCpv(upstream_cpv, latest_cpv) > 0:
        latest_cpv = upstream_cpv

    return latest_cpv

  def _CopyUpstreamPackage(self, info):
    """Upgrades package described by |info| by copying from usptream.

    Returns:
      True if the packages was upgraded, False otherwise.
    """
    latest_cpv = info['latest_cpv']
    if not latest_cpv:
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
    """Updates |info| with latest_cpv, performs upgrade if applicable.

    The upgrade is performed only if the package is outdated and --upgrade
    is specified.
    """
    cpv = info['cpv']
    info['latest_cpv'] = self._FindLatestVersion(cpv)

    cpv_cmp_upstream = None
    info['upgraded'] = False
    if info['latest_cpv']:
      # cpv_cmp_upstream values: 0 = current, >0 = outdated, <0 = futuristic!
      cpv_cmp_upstream = Upgrader._CmpCpv(info['latest_cpv'], cpv)

      if (self._options.upgrade and info['overlay'].startswith('portage') and
          cpv_cmp_upstream > 0):
        info['upgraded'] = self._CopyUpstreamPackage(info)

    # Print details for this package
    if cpv_cmp_upstream is None:
      state = UpgradeTable.STATE_UNKNOWN
    elif cpv_cmp_upstream > 0:
      # Upstream upgrade available.  Note that it is possible that the local
      # package has also been patched.
      # TODO(mtennant): A check for when package is locally patched and
      # uprev'ed upstream.
      state = UpgradeTable.STATE_NEEDS_UPGRADE
    elif cpv_cmp_upstream < 0:
      # Local package is patched from upstream package
      state = UpgradeTable.STATE_PATCHED
    else:
      state = UpgradeTable.STATE_CURRENT

    action_taken = UpgradeTable.ACTION_NONE
    if info['upgraded']: action_taken = UpgradeTable.ACTION_UPGRADED

    # Print a brief one-line report
    if action_taken == UpgradeTable.ACTION_UPGRADED:
      action_stat = ' (UPGRADED)'
    else:
      action_stat = ''
    up_stat = {UpgradeTable.STATE_UNKNOWN: ' no stable package found upstream!',
               UpgradeTable.STATE_NEEDS_UPGRADE: ' -> %s' % info['latest_cpv'],
               UpgradeTable.STATE_PATCHED: ' <- %s' % info['latest_cpv'],
               UpgradeTable.STATE_CURRENT: ' (current)',
               }[state]
    print '[%s] %s%s%s' % (info['overlay'], info['cpv'],
                           up_stat, action_stat)

    # Create a table row to represent this package.
    # Recreate list of packages that this cpv depends on.
    needslist = self._deps_graph[cpv]['needs'].keys()
    upstream_ver = 'N/A'
    if info['latest_cpv']:
      upstream_ver = Upgrader._GetVerRevFromCpv(info['latest_cpv'])

    # Translate to table column values:
    self._table.AppendRow({UpgradeTable.COL_PACKAGE: info['package'],
                           UpgradeTable.COL_OVERLAY: info['overlay'],
                           UpgradeTable.COL_CURRENT_VER: info['version_rev'],
                           UpgradeTable.COL_UPSTREAM_VER: upstream_ver,
                           UpgradeTable.COL_STATE: state,
                           UpgradeTable.COL_DEPENDS_ON: ' '.join(needslist),
                           UpgradeTable.COL_ACTION_TAKEN: action_taken,
                           })

  def _OpenFileForWrite(self, file):
    """If |file| not None, open for writing."""
    try:
      if file:
        return open(file, 'w')
    except IOError as ex:
      print("Unable to open %s for write: %s" % (file, str(ex)))
      return None

  def _WriteTableFiles(self, csv=None, html=None):
    """Write table to |csv| and/or |html| files, if requested."""
    # First sort the table, putting packages of greater interest toward the top.
    def rowkey(row):
      # First key is for COL_STATE value.
      state_val = row[UpgradeTable.COL_STATE]
      key1 = {UpgradeTable.STATE_NEEDS_UPGRADE: 0,
              UpgradeTable.STATE_UNKNOWN: 1,
              UpgradeTable.STATE_PATCHED: 2,
              }.get(state_val, 4)

      # Next key is for overlay.
      key2 = row[UpgradeTable.COL_OVERLAY]

      # Next key is for package name, which is unique final decider.
      key3 = row[UpgradeTable.COL_PACKAGE]

      return (key1, key2, key3)
    self._table.Sort(rowkey)

    if csv:
      filehandle = self._OpenFileForWrite(csv)
      if filehandle:
        self._table.WriteCSV(filehandle)
        filehandle.close()
    if html:
      filehandle = self._OpenFileForWrite(html)
      if filehandle:
        self._table.WriteHTML(filehandle)
        filehandle.close()

  def _UpgradePackages(self, infolist):
    """Given a list of cpv info maps, adds the latest_cpv to the infos."""
    self._table.Clear()
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

  def _GenParallelEmergeArgv(self):
    """Creates an argv for parallel_emerge based on current options."""
    argv = ['--emptytree', '--pretend']
    if self._options.board:
      argv.append('--board=%s' % self._options.board)
    if not self._options.verbose:
      argv.append('--quiet')
    if self._options.rdeps:
      argv.append('--root-deps=rdeps')
    argv.extend(self._args)

    return argv

  def _GetCurrentVersions(self):
    """Returns a list of cpvs of the current package dependencies.

    The returned list is ordered such that the dependencies of any mentioned
    cpv occur later in the list."""
    argv = self._GenParallelEmergeArgv()

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)

    deps_tree, deps_info = deps.GenDependencyTree()
    self._deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)

    return Upgrader._GetPreOrderDepGraph(self._deps_graph)

  def _GetInfoListWithOverlays(self, cpvlist):
    """Returns a list of cpv/overlay info maps corresponding to |cpvlist|."""
    infolist = []
    for cpv in cpvlist:
      # No need to report or try to upgrade chromeos-base packages.
      if cpv.startswith('chromeos-base/'): continue
      # TODO(petkov): Use internal portage utilities to find the overlay instead
      # of equery to improve performance, if possible.
      equery = ['equery', 'which', cpv]
      if self._options.board:
        equery[0] = 'equery-%s' % self._options.board
      ebuild_path = cros_lib.RunCommand(equery, print_cmd=self._options.verbose,
                                        redirect_stdout=True).output.strip()
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      ver_rev = pv.replace(pn + '-', '')
      infolist.append({'cpv': cpv, 'overlay': overlay,
                       'package': '%s/%s' % (cat, pn), 'version_rev': ver_rev,
                       'category': cat, 'package_name': pn, 'package_ver': pv})

    return infolist

  def Run(self):
    """Runs the upgrader based on the supplied options and arguments.

    Currently just lists all package dependencies in pre-order along with
    potential upgrades."""
    cpvlist = self._GetCurrentVersions()
    infolist = self._GetInfoListWithOverlays(cpvlist)
    self._UpgradePackages(infolist)
    self._WriteTableFiles(csv=self._options.csv_file,
                          html=self._options.html_file)

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
  parser.add_option('--to_csv', dest='csv_file', type='string', action='store',
                    default=None, help="File to write csv-formatted results to")
  parser.add_option('--to_html', dest='html_file',
                    type='string', action='store', default=None,
                    help="File to write html-formatted results to")
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

  # Only the unit tests are allowed to not have the board option set.
  if not options.board:
    parser.print_help()
    cros_lib.Die('board is required')

  if not args:
    parser.print_help()
    cros_lib.Die('no packages provided')

  # If upstream portage is provided, verify that it is a valid directory.
  if options.upstream and not os.path.isdir(options.upstream):
    parser.print_help()
    cros_lib.Die('argument to --upstream must be a valid directory')

  upgrader = Upgrader(options, args)
  upgrader.Run()


if __name__ == '__main__':
  main()
