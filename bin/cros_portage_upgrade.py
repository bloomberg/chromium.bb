#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Perform various tasks related to updating Portage packages."""

import logging
import optparse
import os
import parallel_emerge
import portage
import shutil
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib
import chromite.lib.table as table

class UpgradeTable(table.Table):
  """Class to represent upgrade data in memory, can be written to csv/html."""

  # Column names.  Note that 'ARCH' is replaced with a real arch name when
  # these are accessed as attributes off an UpgradeTable object.
  COL_PACKAGE = 'Package'
  COL_SLOT = 'Slot'
  COL_OVERLAY = 'Overlay'
  COL_CURRENT_VER = 'Current ARCH Version'
  COL_STABLE_UPSTREAM_VER = 'Stable Upstream ARCH Version'
  COL_LATEST_UPSTREAM_VER = 'Latest Upstream ARCH Version'
  COL_STATE = 'State On ARCH'
  COL_DEPENDS_ON = 'Dependencies On ARCH'
  COL_TARGET = 'Root Target'
  COL_ACTION_TAKEN = 'Action Taken'

  # COL_STATE values should be one of the following:
  STATE_UNKNOWN = 'unknown'
  STATE_NEEDS_UPGRADE = 'needs upgrade'
  STATE_PATCHED = 'patched locally'
  STATE_DUPLICATED = 'duplicated locally'
  STATE_NEEDS_UPGRADE_AND_PATCHED = 'needs upgrade and patched locally'
  STATE_NEEDS_UPGRADE_AND_DUPLICATED = 'needs upgrade and duplicated locally'
  STATE_CURRENT = 'current'

  @staticmethod
  def GetColumnName(col, arch=None):
    """Translate from generic column name to specific given |arch|."""
    if arch:
      return col.replace('ARCH', arch)

  def __init__(self, arch):
    self._arch = arch

    # These constants serve two roles, for both csv and html table output:
    # 1) Restrict which column names are valid.
    # 2) Specify the order of those columns.
    columns = [self.COL_PACKAGE,
               self.COL_SLOT,
               self.COL_OVERLAY,
               self.COL_CURRENT_VER,
               self.COL_STABLE_UPSTREAM_VER,
               self.COL_LATEST_UPSTREAM_VER,
               self.COL_STATE,
               self.COL_DEPENDS_ON,
               self.COL_TARGET,
               self.COL_ACTION_TAKEN,
               ]

    table.Table.__init__(self, columns)

  def __getattribute__(self, name):
    """When accessing self.COL_*, substitute ARCH name."""
    if name.startswith('COL_'):
      text = getattr(UpgradeTable, name)
      return UpgradeTable.GetColumnName(text, arch=self._arch)
    else:
      return object.__getattribute__(self, name)

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
          elif (val == self.STATE_NEEDS_UPGRADE_AND_DUPLICATED or
                val == self.STATE_NEEDS_UPGRADE_AND_PATCHED):
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

    self._arch = Upgrader._FindBoardArch(self._options.board)
    self._table = UpgradeTable(self._arch)

    self._porttree = None
    self._emptydir = None
    self._deps_graph = None

  @staticmethod
  def _FindBoardArch(board):
    """Return the architecture for a given board name."""
    # Leverage Portage 'portageq' tool to do this.
    cmd = ['portageq-%s' % board, 'envvar', 'ARCH']
    cmd_result = cros_lib.RunCommand(cmd, print_cmd=False,
                                     redirect_stdout=True, exit_code=True)
    if cmd_result.returncode == 0:
      return cmd_result.output.strip()
    else:
      return None

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
  def _CmpCpv(cpv1, cpv2):
    """Returns standard cmp result between |cpv1| and |cpv2|."""
    return portage.versions.pkgcmp(portage.versions.pkgsplit(cpv1),
                                   portage.versions.pkgsplit(cpv2))

  @staticmethod
  def _GetVerRevFromCpv(cpv):
    """Returns just the version-revision string from a full |cpv|."""
    if not cpv:
      return None

    (cat, pn, version, rev) = portage.versions.catpkgsplit(cpv)
    if rev != 'r0':
      return '%s-%s' % (version, rev)
    else:
      return version

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

  def _FindUpstreamCPV(self, pkg, arch, unstable_ok=False):
    """Returns latest cpv in |_upstream_repo| that matches |pkg|.

    The |pkg| argument can specify as much or as little of the full CPV
    syntax as desired, exactly as accepted by the Portage 'equery' command.
    To find whether an exact version exists upstream specify the full
    CPV.  To find the latest version specify just the category and package
    name.

    To filter by architecture keyword (e.g. 'arm' or 'x86'), specify
    the |arch| argument.  By default, only ebuilds stable on that arch
    will be accepted.  To accept unstable ebuilds, set |unstable_ok| to True.

    Returns upstream cpv, if found.
    """
    envvars = {'PORTDIR': self._upstream_repo,
               'PORTAGE_CONFIGROOT': self._emptydir,
               }
    if arch:
      if unstable_ok:
        envvars['ACCEPT_KEYWORDS'] =  arch + ' ~' + arch
      else:
        envvars['ACCEPT_KEYWORDS'] =  arch

    # Point equery to the upstream source to get latest version for keywords.
    equery = ['equery', 'which', pkg ]
    cmd_result = cros_lib.RunCommand(equery, extra_env=envvars,
                                     print_cmd=self._options.verbose,
                                     exit_code=True, error_ok=True,
                                     redirect_stdout=True,
                                     combine_stdout_stderr=True,
                                     )
    if cmd_result.returncode == 0:
      ebuild_path = cmd_result.output.strip()
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      return os.path.join(cat, pv)
    else:
      return None

  def _CopyUpstreamPackage(self, upstream_cpv):
    """Upgrades package in |upstream_cpv| to the version in |upstream_cpv|.

    Returns:
      The upstream_cpv if the package was upgraded, None otherwise.
    """
    if not upstream_cpv:
      return None

    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(upstream_cpv)
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
                              upstream_cpv.split('/')[1] + '.ebuild'), pkgdir)
    self._RunGit(self._stable_repo, 'add ' + catpkgname)
    return upstream_cpv

  def _GetPackageUpgradeState(self, info, cpv_cmp_upstream):
    """Return state value for package in |info| given |cpv_cmp_upstream|.

    The value in |cpv_cmp_upstream| represents a comparison of cpv version
    and the upstream version, where:
    0 = current, >0 = outdated, <0 = futuristic!
    """
    # See whether this specific cpv exists upstream.
    cpv = info['cpv']
    cpv_exists_upstream = bool(self._FindUpstreamCPV(cpv, self._arch,
                                                     unstable_ok=True))
    # Convention is that anything not in portage overlay has been altered.
    locally_patched = info['overlay'] != 'portage'
    locally_duplicated = locally_patched and cpv_exists_upstream

    # Gather status details for this package
    if cpv_cmp_upstream is None:
      state = UpgradeTable.STATE_UNKNOWN
    elif cpv_cmp_upstream > 0:
      if locally_duplicated:
        state = UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED
      elif locally_patched:
        state = UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED
      else:
        state = UpgradeTable.STATE_NEEDS_UPGRADE
    elif locally_duplicated:
      state = UpgradeTable.STATE_DUPLICATED
    elif locally_patched:
      state = UpgradeTable.STATE_PATCHED
    else:
      state = UpgradeTable.STATE_CURRENT

    return state

  def _PrintPackageLine(self, info):
    """Print a brief one-line report of package status."""
    upstream_cpv = info['upstream_cpv']
    if info['upgraded_cpv']:
      action_stat = ' (UPGRADED)'
    else:
      action_stat = ''
    up_stat = {UpgradeTable.STATE_UNKNOWN: ' no package found upstream!',
               UpgradeTable.STATE_NEEDS_UPGRADE: ' -> %s' % upstream_cpv,
               UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED:
               ' <-> %s' % upstream_cpv,
               UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED:
               ' (locally duplicated) <-> %s' % upstream_cpv,
               UpgradeTable.STATE_PATCHED: ' <- %s' % upstream_cpv,
               UpgradeTable.STATE_DUPLICATED: ' (locally duplicated)',
               UpgradeTable.STATE_CURRENT: ' (current)',
               }[info['state']]
    print '[%s] %s%s%s' % (info['overlay'], info['cpv'],
                           up_stat, action_stat)

  def _AppendPackageRow(self, info):
    """Add a row to status table for the package in |info|."""
    cpv = info['cpv']
    upstream_cpv = info['upstream_cpv']
    upstream_ver = Upgrader._GetVerRevFromCpv(upstream_cpv)
    upgraded_cpv = info['upgraded_cpv']

    # Prepare defaults for columns without values for this row.
    action_taken = ''
    if upgraded_cpv:
      action_taken = 'upgraded to %s' % upstream_ver
    depslist = sorted(self._deps_graph[cpv]['needs'].keys()) # dependencies
    stable_up_ver = Upgrader._GetVerRevFromCpv(info['stable_upstream_cpv'])
    if not stable_up_ver:
      stable_up_ver = 'N/A'
    latest_up_ver = Upgrader._GetVerRevFromCpv(info['latest_upstream_cpv'])
    if not latest_up_ver:
      latest_up_ver = 'N/A'
    row = {self._table.COL_PACKAGE: info['package'],
           self._table.COL_SLOT: info['slot'],
           self._table.COL_OVERLAY: info['overlay'],
           self._table.COL_CURRENT_VER: info['version_rev'],
           self._table.COL_STABLE_UPSTREAM_VER: stable_up_ver,
           self._table.COL_LATEST_UPSTREAM_VER: latest_up_ver,
           self._table.COL_STATE: info['state'],
           self._table.COL_DEPENDS_ON: ' '.join(depslist),
           self._table.COL_TARGET: ' '.join(self._args),
           self._table.COL_ACTION_TAKEN: action_taken,
           }
    self._table.AppendRow(row)

  def _UpgradePackage(self, info):
    """Gathers upgrade status for pkg, performs upgrade if requested.

    The upgrade is performed only if the package is outdated and --upgrade
    is specified.

    Regardless, the following entries in |info| dict are filled in:
    stable_upstream_cpv
    latest_upstream_cpv
    upstream_cpv (one of the above, depending on --stable-only option)
    upgrade_cpv (if upgrade performed)
    """
    cpv = info['cpv']
    catpkg = Upgrader._GetCatPkgFromCpv(cpv)
    info['stable_upstream_cpv'] = self._FindUpstreamCPV(catpkg, self._arch)
    info['latest_upstream_cpv'] = self._FindUpstreamCPV(catpkg, self._arch,
                                                        unstable_ok=True)

    # The upstream version can be either latest stable or latest overall.
    if self._options.stable_only:
      upstream_cpv = info['stable_upstream_cpv']
    else:
      upstream_cpv = info['latest_upstream_cpv']
    info['upstream_cpv'] = upstream_cpv

    # Perform the actual upgrade, if requested.
    cpv_cmp_upstream = None
    info['upgraded_cpv'] = False
    if upstream_cpv:
      # cpv_cmp_upstream values: 0 = current, >0 = outdated, <0 = futuristic!
      cpv_cmp_upstream = Upgrader._CmpCpv(upstream_cpv, cpv)

      if (self._options.upgrade and info['overlay'].startswith('portage') and
          cpv_cmp_upstream > 0):
        info['upgraded_cpv'] = self._CopyUpstreamPackage(upstream_cpv)

    info['state'] = self._GetPackageUpgradeState(info, cpv_cmp_upstream)

    # Print a quick summary of package status.
    self._PrintPackageLine(info)

    # Add a row to status table for this package
    self._AppendPackageRow(info)

  def _OpenFileForWrite(self, filepath):
    """If |file| not None, open for writing."""
    try:
      if filepath:
        return open(filepath, 'w')
    except IOError as ex:
      print("Unable to open %s for write: %s" % (filepath, str(ex)))
      return None

  def _WriteTableFiles(self, csv=None, html=None):
    """Write table to |csv| and/or |html| files, if requested."""
    # Sort the table by package name, then slot
    def PkgSlotSort(row):
      return (row[self._table.COL_PACKAGE], row[self._table.COL_SLOT])
    self._table.Sort(PkgSlotSort)

    if csv:
      filehandle = self._OpenFileForWrite(csv)
      if filehandle:
        print "Writing package status as csv to %s" % csv
        hiddencols = None
        if not self._options.upgrade:
          hiddencols = set([self._table.COL_ACTION_TAKEN])
        self._table.WriteCSV(filehandle, hiddencols)
        filehandle.close()
    if html:
      filehandle = self._OpenFileForWrite(html)
      if filehandle:
        print "Writing package status as html to %s" % html
        self._table.WriteHTML(filehandle)
        filehandle.close()

  def _UpgradePackages(self, infolist):
    """Given a list of cpv info maps, adds the upstream cpv to the infos."""
    # An empty directory is needed to trick equery later.
    self._emptydir = tempfile.mkdtemp()

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
        if info['upgraded_cpv']:
          message += 'Upgrade %s to %s\n' % (info['cpv'], info['upgraded_cpv'])
      if message:
        message = 'Upgrade Portage packages\n\n' + message
        message += '\nBUG=<fill-in>'
        message += '\nTEST=<fill-in>'
        self._RunGit(self._stable_repo, "commit -am '%s'" % message)
        cros_lib.Info('Use "git commit --amend" to update the commit message.')
    finally:
      if not self._options.upstream:
        self._RunGit(self._upstream_repo, 'checkout %s cros/master' % dash_q)
      os.rmdir(self._emptydir)

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

  def _SetPortTree(self, settings, trees):
    """Set self._porttree from portage |settings| and |trees|."""
    root = settings["ROOT"]
    self._porttree = trees[root]['porttree']

  def _GetPortageDBAPI(self):
    """Retrieve the Portage dbapi object, if available."""
    try:
      return self._porttree.dbapi
    except AttributeError:
      return None

  def _GetCurrentVersions(self):
    """Returns a list of cpvs of the current package dependencies.

    The returned list is ordered such that the dependencies of any mentioned
    cpv occur later in the list."""
    argv = self._GenParallelEmergeArgv()

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)

    deps_tree, deps_info = deps.GenDependencyTree()

    self._SetPortTree(deps.emerge.settings, deps.emerge.trees)

    self._deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)
    return Upgrader._GetPreOrderDepGraph(self._deps_graph)

  def _GetInfoListWithOverlays(self, cpvlist):
    """Returns a list of cpv/overlay info maps corresponding to |cpvlist|."""
    infolist = []
    for cpv in cpvlist:
      # No need to report or try to upgrade chromeos-base packages.
      if cpv.startswith('chromeos-base/'): continue

      dbapi = self._GetPortageDBAPI()
      ebuild_path = dbapi.findname2(cpv)[0]
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      ver_rev = pv.replace(pn + '-', '')
      slot, = dbapi.aux_get(cpv, ['SLOT'])

      infolist.append({'cpv': cpv, 'slot': slot, 'overlay': overlay,
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
  """Main function."""
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
  parser.add_option('--stable-only', dest='stable_only', action='store_true',
                    default=False,
                    help="Use only stable upstream ebuilds for upgrades")
  parser.add_option('--to-csv', dest='csv_file', type='string', action='store',
                    default=None, help="File to write csv-formatted results to")
  parser.add_option('--to-html', dest='html_file',
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
