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
import re
import shutil
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
import chromite.lib.cros_build_lib as cros_lib
import chromite.lib.upgrade_table as utable
import merge_package_status as mps

# TODO(mtennant): I see comments next to cros_build_lib.Info,Warning,Die that
# they are deprecated and to be replaced by those in operation module.

# TODO(mtennant): Use this class to replace the 'info' dictionary used
# throughout. In the meantime, it simply serves as documentation for
# the values in that dictionary.
class PInfo(object):
  """Class to accumulate package info during upgrade process.

  This class is basically a formalized dictionary."""

  __slots__ = [
      'category',            # Package category only
      'cpv',                 # Current full cpv (revision included)
      'emerge_ok',           # True if upgraded_cpv is emergeable
      'emerge_output',       # Output from pretend emerge of upgraded_cpv
      'emerge_stable',       # TODO: get rid of this one
      'latest_upstream_cpv', # Latest (non-stable ok) upstream cpv
      'overlay',             # Overlay package currently in
      'package',             # category/package_name
      'package_name',        # The 'p' in 'cpv'
      'package_ver',         # The 'pv' in 'cpv'
      'slot',                # Current package slot
      'stable_upstream_cpv', # Latest stable upstream cpv
      'state',               # One of utable.UpgradeTable.STATE_*
      'upgraded_cpv',        # If upgraded, it is to this cpv
      'upstream_cpv',        # latest/stable upstream cpv according to request
      'version_rev',         # Just revision (e.g. 'r1').  '' if no revision
      ]

class Upgrader(object):
  """A class to perform various tasks related to updating Portage packages."""

  UPSTREAM_OVERLAY_NAME = 'portage'
  STABLE_OVERLAY_NAME = 'portage-stable'
  CROS_OVERLAY_NAME = 'chromiumos-overlay'
  HOST_BOARD = 'amd64-host'
  OPT_SLOTS = ['amend', 'csv_file', 'html_file', 'rdeps',
               'upgrade', 'upgrade_deep', 'upstream', 'unstable_ok', 'verbose']

  __slots__ = ['_amend',        # Boolean to use --amend with upgrade commit
               '_args',         # Commandline arguments (all portage targets)
               '_curr_arch',    # Architecture for current board run
               '_curr_board',   # Board for current board run
               '_curr_table',   # Package status for current board run
               '_cros_overlay', # Path to chromiumos-overlay repo
               '_csv_file',     # File path for writing csv output
               '_deps_graph',   # Dependency graph from portage
               '_emptydir',     # Path to temporary empty directory
               '_html_file',    # File path for writing html output
               '_master_archs', # Set. Archs of tables merged into master_table
               '_master_cnt',   # Number of tables merged into master_table
               '_master_table', # Merged table from all board runs
               '_porttree',     # Reference to portage porttree object
               '_rdeps',        # Boolean, if True pass --root-deps=rdeps
               '_stable_repo',  # Path to portage-stable
               '_stable_repo_status', # git status report at start of run
               '_targets',      # Processed list of portage targets
               '_upgrade',      # Boolean indicating upgrade requested
               '_upgrade_cnt',  # Num pkg upgrades in this run (all boards)
               '_upgrade_deep', # Boolean indicating upgrade_deep requested
               '_upstream',     # User-provided path to upstream repo
               '_upstream_repo',# Path to upstream portage repo
               '_unstable_ok',  # Boolean to allow unstable upstream also
               '_verbose',      # Boolean
               ]

  def __init__(self, options=None, args=None):
    self._args = args
    self._targets = mps.ProcessTargets(args)

    self._master_table = None
    self._master_cnt = 0
    self._master_archs = set()
    self._upgrade_cnt = 0

    self._stable_repo = os.path.join(options.srcroot, 'third_party',
                                     self.STABLE_OVERLAY_NAME)
    self._upstream_repo = options.upstream
    if not self._upstream_repo:
      self._upstream_repo = os.path.join(options.srcroot, 'third_party',
                                         self.UPSTREAM_OVERLAY_NAME)
    self._cros_overlay = os.path.join(options.srcroot, 'third_party',
                                      self.CROS_OVERLAY_NAME)

    # Save options needed later.
    for opt in self.OPT_SLOTS:
      setattr(self, '_' + opt, getattr(options, opt, None))

    self._porttree = None
    self._emptydir = None
    self._deps_graph = None

  def _GetPkgKeywordsFile(self):
    """Return the path to the package.keywords file in chromiumos-overlay."""
    return '%s/profiles/targets/chromeos/package.keywords' % self._cros_overlay

  def _SaveStatusOnStableRepo(self):
    """Get the 'git status' for everything in |self._stable_repo|.

    The results are saved in a dict at self._stable_repo_status where each key
    is a file path rooted at |self._stable_repo|, and the value is the status
    for that file as returned by 'git status -s'.  (e.g. 'A' for 'Added').
    """
    result = self._RunGit(self._stable_repo, 'status -s', redirect_stdout=True)
    if result.returncode == 0:
      statuses = {}
      for line in result.output.strip().split('\n'):
        if not line:
          continue

        linesplit = line.split()
        (status, path) = linesplit[0], linesplit[1]
        statuses[path] = status

      self._stable_repo_status = statuses
    else:
      self._stable_repo_status = None

  def _CheckStableRepoOnBranch(self):
    """Raise exception if |self._stable_repo| is not on a branch now."""
    # TODO(mtennant): Create the branch as needed instead.
    result = self._RunGit(self._stable_repo, 'branch', redirect_stdout=True)
    if result.returncode == 0:
      for line in result.output.split('\n'):
        match = re.search(r'^\*\s+(.+)$', line)
        if match:
          # Found current branch, see if it is a real branch.
          branch = match.group(1)
          if branch != '(no branch)':
            return
          raise RuntimeError("To perform upgrade, %s must be on a branch." %
                             self._stable_repo)

    raise RuntimeError("Unable to determine whether %s is on a branch." %
                       self._stable_repo)

  def _PkgUpgradeRequested(self, info):
    """Return True if upgrade of pkg in |info| hash was requested by user."""
    if self._upgrade_deep:
      return True

    if self._upgrade:
      # See if this package was directly requested at command line.
      for pkg in self._args:
        if pkg == info['package'] or pkg == info['package_name']:
          return True

    return False

  @staticmethod
  def _FindBoardArch(board):
    """Return the architecture for a given board name."""
    # Host is a special case
    if board == Upgrader.HOST_BOARD:
      return 'amd64'

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

  def _RunGit(self, repo, command, redirect_stdout=False):
    """Runs |command| in the git |repo|.

    This leverages the cros_build_lib.RunCommand function.  The
    |redirect_stdout| argument is passed to that function.

    Returns a Result object as documented by cros_build_lib.RunCommand.
    Most usefully, the result object has a .output attribute containing
    the output from the command (if |redirect_stdout| was True).
    """
    cmdline = ['/bin/sh', '-c', 'cd %s && git -c core.pager=" " %s' %
               (repo, command)]
    result = cros_lib.RunCommand(cmdline, exit_code=True,
                                 print_cmd=self._verbose,
                                 redirect_stdout=redirect_stdout)
    return result

  def _SplitEBuildPath(self, ebuild_path):
    """Split a full ebuild path into (overlay, cat, pn, pv)."""
    (ebuild_path, ebuild) = os.path.splitext(ebuild_path)
    (ebuild_path, pv) = os.path.split(ebuild_path)
    (ebuild_path, pn) = os.path.split(ebuild_path)
    (ebuild_path, cat) = os.path.split(ebuild_path)
    (ebuild_path, overlay) = os.path.split(ebuild_path)
    return (overlay, cat, pn, pv)

  def _GenPortageEnvvars(self, arch, unstable_ok, portdir=None,
                         portage_configroot=None):
    """Returns dictionary of envvars for running portage tools.

    If |arch| is set, then ACCEPT_KEYWORDS will be included and set
    according to |unstable_ok|.

    PORTDIR is set to |portdir| value, if not None.
    PORTAGE_CONFIGROOT is set to |portage_configroot| value, if not None.
    """
    envvars = {}
    if arch:
      if unstable_ok:
        envvars['ACCEPT_KEYWORDS'] =  arch + ' ~' + arch
      else:
        envvars['ACCEPT_KEYWORDS'] =  arch

    if portdir is not None:
      envvars['PORTDIR'] = portdir
    if portage_configroot is not None:
      envvars['PORTAGE_CONFIGROOT'] = portage_configroot

    return envvars

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
    envvars = self._GenPortageEnvvars(arch, unstable_ok,
                                      portdir=self._upstream_repo,
                                      portage_configroot=self._emptydir)

    # Point equery to the upstream source to get latest version for keywords.
    equery = ['equery', 'which', pkg ]
    cmd_result = cros_lib.RunCommand(equery, extra_env=envvars,
                                     print_cmd=self._verbose,
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

  def _IsEmergeable(self, cpv, stable_only):
    """Indicate whether |cpv| can be emerged on current board.

    This essentially runs emerge with the --pretend option to verify
    that all dependencies for this package version are satisfied.

    The |stable_only| argument determines whether an unstable version
    is acceptable.

    Return tuple with two elements:
    [0] True if |cpv| can be emerged.
    [1] Output from the emerge command.
    """
    envvars = self._GenPortageEnvvars(self._curr_arch, not stable_only)

    emerge = 'emerge'
    if self._curr_board != self.HOST_BOARD:
      emerge = 'emerge-%s' % self._curr_board

    cmd = [emerge, '-p', '=' + cpv]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )

    return (result.returncode == 0, ' '.join(cmd), result.output)

  def _VerifyEbuildOverlay(self, cpv, overlay, stable_only):
    """Raises exception if ebuild for |cpv| is not from |overlay|.

    Essentially, this verifies that the upgraded ebuild in portage-stable
    is indeed the one being picked up, rather than some other ebuild with
    the same version in another overlay.
    """
    # Further explanation: this check should always pass, but might not
    # if the copy/upgrade from upstream did not work and
    # src/third-party/portage is being used as temporary upstream copy via
    # 'git checkout cros/gentoo'.  This is just a sanity check.
    envvars = self._GenPortageEnvvars(self._curr_arch, not stable_only)

    equery = 'equery'
    if self._curr_board != self.HOST_BOARD:
      equery = 'equery-%s' % self._curr_board

    cmd = [equery, 'which', cpv]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )
    ebuild_path = result.output.strip()
    (ovrly, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
    if ovrly != overlay:
      raise RuntimeError('Somehow ebuild for %s is not coming from %s:\n %s' %
                         (cpv, overlay, ebuild_path))

  def _PkgUpgradeStaged(self, upstream_cpv):
    """Return True if package upgrade is already staged."""
    if not upstream_cpv:
      return False

    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(upstream_cpv)
    ebuild = upstream_cpv.replace(cat + '/', '') + '.ebuild'
    ebuild_relative_path = os.path.join(cat, pkgname, ebuild)

    status = self._stable_repo_status.get(ebuild_relative_path, None)
    if status and 'A' == status:
      return True

    return False

  def _CopyUpstreamPackage(self, upstream_cpv):
    """Upgrades package in |upstream_cpv| to the version in |upstream_cpv|.

    Returns:
      The upstream_cpv if the package was upgraded, None otherwise.
    """
    if not upstream_cpv:
      return None

    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(upstream_cpv)
    ebuild = upstream_cpv.replace(cat + '/', '') + '.ebuild'
    catpkgsubdir = os.path.join(cat, pkgname)
    pkgdir = os.path.join(self._stable_repo, catpkgsubdir)
    upstream_pkgdir = os.path.join(self._upstream_repo, cat, pkgname)

    # If pkgdir already exists, remove everything except ebuilds that
    # correspond to ebuilds that are also upstream.
    if os.path.exists(pkgdir):
      items = os.listdir(pkgdir)
      for item in items:
        src = os.path.join(upstream_pkgdir, item)
        if not item.endswith('.ebuild') or not os.path.exists(src):
          self._RunGit(pkgdir, 'rm -rf ' + item, redirect_stdout=True)
    else:
      os.makedirs(pkgdir)

    # Grab all non-ebuilds from upstream plus the specific ebuild requested.
    if os.path.exists(upstream_pkgdir):
      items = os.listdir(upstream_pkgdir)
      for item in items:
        src = os.path.join(upstream_pkgdir, item)
        dst = os.path.join(pkgdir, item)
        if not item.endswith('.ebuild') or item == ebuild:
          if os.path.isdir(src):
            shutil.copytree(src, dst)
          else:
            shutil.copy2(src, dst)
    self._RunGit(self._stable_repo, 'add ' + catpkgsubdir)

    return upstream_cpv

  def _GetPackageUpgradeState(self, info, cpv_cmp_upstream):
    """Return state value for package in |info| given |cpv_cmp_upstream|.

    The value in |cpv_cmp_upstream| represents a comparison of cpv version
    and the upstream version, where:
    0 = current, >0 = outdated, <0 = futuristic!
    """
    # See whether this specific cpv exists upstream.
    cpv = info['cpv']
    cpv_exists_upstream = bool(self._FindUpstreamCPV(cpv, self._curr_arch,
                                                     unstable_ok=True))

    # Convention is that anything not in portage overlay has been altered.
    # TODO(mtennant): Distinguish between 'portage' and 'portage-stable'
    # overlays in status reports.  Use something like:
    # locally_pinned = overlay == self.STABLE_OVERLAY_NAME
    overlay = info['overlay']
    locally_patched = (overlay != self.UPSTREAM_OVERLAY_NAME and
                       overlay != self.STABLE_OVERLAY_NAME)
    locally_duplicated = locally_patched and cpv_exists_upstream

    # Gather status details for this package
    if cpv_cmp_upstream is None:
      state = utable.UpgradeTable.STATE_UNKNOWN
    elif cpv_cmp_upstream > 0:
      if locally_duplicated:
        state = utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED
      elif locally_patched:
        state = utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED
      else:
        state = utable.UpgradeTable.STATE_NEEDS_UPGRADE
    elif locally_duplicated:
      state = utable.UpgradeTable.STATE_DUPLICATED
    elif locally_patched:
      state = utable.UpgradeTable.STATE_PATCHED
    else:
      state = utable.UpgradeTable.STATE_CURRENT

    return state

  # TODO(mtennant): Generate output from finished table instead.
  def _PrintPackageLine(self, info):
    """Print a brief one-line report of package status."""
    upstream_cpv = info['upstream_cpv']
    if info['upgraded_cpv']:
      if info['emerge_ok']:
        action_stat = ' (UPGRADED, EMERGE WORKS)'
      else:
        action_stat = ' (UPGRADED, BUT EMERGE FAILS)'
    else:
      action_stat = ''
    up_stat = {utable.UpgradeTable.STATE_UNKNOWN: ' no package found upstream!',
               utable.UpgradeTable.STATE_NEEDS_UPGRADE: ' -> %s' % upstream_cpv,
               utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_PATCHED:
               ' <-> %s' % upstream_cpv,
               utable.UpgradeTable.STATE_NEEDS_UPGRADE_AND_DUPLICATED:
               ' (locally duplicated) <-> %s' % upstream_cpv,
               utable.UpgradeTable.STATE_PATCHED: ' <- %s' % upstream_cpv,
               utable.UpgradeTable.STATE_DUPLICATED: ' (locally duplicated)',
               utable.UpgradeTable.STATE_CURRENT: ' (current)',
               }[info['state']]
    print '[%s] %s%s%s' % (info['overlay'], info['cpv'],
                           up_stat, action_stat)

  def _AppendPackageRow(self, info):
    """Add a row to status table for the package in |info|."""
    cpv = info['cpv']
    upstream_cpv = info['upstream_cpv']
    upstream_ver = Upgrader._GetVerRevFromCpv(upstream_cpv)
    upgraded_cpv = info['upgraded_cpv']

    upgraded_ver = ''
    if upgraded_cpv:
      if info['emerge_ok']:
        upgraded_ver = upstream_ver
      else:
        upgraded_ver = '(emerge fails)' + upstream_ver

    depslist = sorted(self._deps_graph[cpv]['needs'].keys()) # dependencies
    stable_up_ver = Upgrader._GetVerRevFromCpv(info['stable_upstream_cpv'])
    if not stable_up_ver:
      stable_up_ver = 'N/A'
    latest_up_ver = Upgrader._GetVerRevFromCpv(info['latest_upstream_cpv'])
    if not latest_up_ver:
      latest_up_ver = 'N/A'

    row = {self._curr_table.COL_PACKAGE: info['package'],
           self._curr_table.COL_SLOT: info['slot'],
           self._curr_table.COL_OVERLAY: info['overlay'],
           self._curr_table.COL_CURRENT_VER: info['version_rev'],
           self._curr_table.COL_STABLE_UPSTREAM_VER: stable_up_ver,
           self._curr_table.COL_LATEST_UPSTREAM_VER: latest_up_ver,
           self._curr_table.COL_STATE: info['state'],
           self._curr_table.COL_DEPENDS_ON: ' '.join(depslist),
           self._curr_table.COL_TARGET: ' '.join(self._targets),
           }

    # Only include if upgrade was involved.  Table may not have this column
    # if upgrade was not requested.
    if upgraded_ver:
      row[self._curr_table.COL_UPGRADED] = upgraded_ver

    self._curr_table.AppendRow(row)

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
    info['stable_upstream_cpv'] = self._FindUpstreamCPV(catpkg, self._curr_arch)
    info['latest_upstream_cpv'] = self._FindUpstreamCPV(catpkg, self._curr_arch,
                                                        unstable_ok=True)

    # The upstream version can be either latest stable or latest overall.
    if not self._unstable_ok:
      upstream_cpv = info['stable_upstream_cpv']
    else:
      upstream_cpv = info['latest_upstream_cpv']
    info['upstream_cpv'] = upstream_cpv

    # Perform the actual upgrade, if requested.
    cpv_cmp_upstream = None
    info['upgraded_cpv'] = None
    if upstream_cpv:
      # cpv_cmp_upstream values: 0 = current, >0 = outdated, <0 = futuristic!
      cpv_cmp_upstream = Upgrader._CmpCpv(upstream_cpv, cpv)

      # Determine whether upgrade of this package is requested.
      if self._PkgUpgradeRequested(info):
        if self._PkgUpgradeStaged(upstream_cpv):
          cros_lib.Info('Determined that %s is already staged.' % upstream_cpv)
          info['upgraded_cpv'] = upstream_cpv
        elif cpv_cmp_upstream > 0:
          cros_lib.Info('Copying %s from upstream.' % upstream_cpv)
          info['upgraded_cpv'] = self._CopyUpstreamPackage(upstream_cpv)

      if info['upgraded_cpv']:
        # Verify that upgraded package can be emerged and save results.
        # Prefer stable if possible, otherwise remember that a keyword
        # change will be needed.
        # TODO(mtennant): Can trim to one emerge by determining whether
        # upstream_cpv is stable or not (compare to latest/stable columns).
        (em_ok_stable,
         em_cmd_stable,
         em_out_stable) = self._IsEmergeable(upstream_cpv, True)
        (em_ok_all,
         em_cmd_all,
         em_out_all) = self._IsEmergeable(upstream_cpv, False)
        if em_ok_stable or not em_ok_all:
          info['emerge_ok'] = em_ok_stable
          info['emerge_cmd'] = em_cmd_stable
          info['emerge_output'] = em_out_stable
          info['emerge_stable'] = True
        else:
          info['emerge_ok'] = em_ok_all
          info['emerge_cmd'] = em_cmd_all
          info['emerge_output'] = em_out_all
          info['emerge_stable'] = False
        if info['emerge_ok']:
          self._VerifyEbuildOverlay(upstream_cpv, self.STABLE_OVERLAY_NAME,
                                    info['emerge_stable'])

    info['state'] = self._GetPackageUpgradeState(info, cpv_cmp_upstream)

    # Print a quick summary of package status.
    self._PrintPackageLine(info)

    # Add a row to status table for this package
    self._AppendPackageRow(info)

    return bool(info['upgraded_cpv'])

  def _CreateCommitMessage(self, upgrade_lines, bug_num=None):
    """Create appropriate git commit message for upgrades in |upgrade_lines|."""
    message = ''
    upgrade_count = len(upgrade_lines)
    upgrade_str = '\n'.join(upgrade_lines)
    if upgrade_count == 1:
      message = 'Upgrade the following Portage package\n\n%s\n' % upgrade_str
    else:
      message = ('Upgrade the following %d Portage packages\n\n%s\n' %
                 (upgrade_count, upgrade_str))

    # The space before <fill-in> (at least for TEST=) fails pre-submit check,
    # which is the intention here.
    if bug_num:
      message += '\nBUG=chromium-os:%s' % bug_num
    else:
      message += '\nBUG= <fill-in>'
    message += '\nTEST= <fill-in>'

    return message

  def _AmendCommitMessage(self, upgrade_lines):
    """Create git commit message combining |upgrade_lines| with last commit."""
    # First get the body of the last commit message.
    git_cmd = 'show --pretty=format:"__BEGIN BODY__%n%b%n__END BODY__"'
    result = self._RunGit(self._stable_repo, git_cmd, redirect_stdout=True)
    match = re.search(r'__BEGIN BODY__\n(.+)__END BODY__',
                      result.output, re.DOTALL)
    if match:
      # Extract the upgrade_lines of last commit.
      body = match.group(1)
      for line in body.split('\n'):
        if line:
          upgrade_lines.append(line)
        else:
          break

    return self._CreateCommitMessage(upgrade_lines)

  def _GiveEmergeResults(self, infolist):
    """Summarize emerge checks, raise RuntimeError if there was a problem."""
    emerge_ok = True
    for info in infolist:
      if info['upgraded_cpv']:
        if info['emerge_ok']:
          cros_lib.Info('Confirmed %s can be emerged on %s after upgrade.' %
                        (info['upgraded_cpv'], self._curr_board))
        else:
          emerge_ok = False
          cros_lib.Warning('Unable to emerge %s after upgrade.\n'
                           'The output of "%s" follows:\n' %
                           (info['upgraded_cpv'], info['emerge_cmd']))
          print info['emerge_output']

    if not emerge_ok:
      raise RuntimeError("One or more emerge failures after upgrade.")

  def _UpgradePackages(self, infolist):
    """Given a list of cpv info maps, adds the upstream cpv to the infos."""
    self._curr_table.Clear()

    for info in infolist:
      if self._UpgradePackage(info):
        self._upgrade_cnt += 1

    try:
      self._GiveEmergeResults(infolist)
    except RuntimeError:
      # Failure to emerge the upgraded package(s) must stop the upgrade, or
      # else the later logic will merrily commit the upgrade changes.
      cros_lib.Die('Failed to complete upgrades on %s (see above).  Address'
                   ' the emerge errors before continuing.\n'
                   'To reset your changes instead::\n'
                   ' cd %s; git reset --hard; cd -' %
                   (self._curr_board, self._stable_repo))
      # Allow the changes to stay staged so that the user can attempt to address
      # the issue (perhaps an edit to package.mask is required, or another
      # package must also be upgraded).

  def _GenParallelEmergeArgv(self):
    """Creates an argv for parallel_emerge based on current options."""
    argv = ['--emptytree', '--pretend']
    if self._curr_board and self._curr_board != self.HOST_BOARD:
      argv.append('--board=%s' % self._curr_board)
    if not self._verbose:
      argv.append('--quiet')
    if self._rdeps:
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
    cpv occur earlier in the list."""
    argv = self._GenParallelEmergeArgv()

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)

    deps_tree, deps_info = deps.GenDependencyTree()

    self._SetPortTree(deps.emerge.settings, deps.emerge.trees)

    self._deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)
    cpv_list = Upgrader._GetPreOrderDepGraph(self._deps_graph)
    cpv_list.reverse()
    return cpv_list

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

  def PrepareToRun(self):
    """Checkout upstream gentoo if necessary, and any other prep steps."""
    # TODO(petkov): Currently portage's master branch is stale so we need to
    # checkout latest upstream. At some point portage's master branch will be
    # upstream so there will be no need to chdir/checkout. At that point we
    # can also fuse this loop into the caller and avoid generating a separate
    # list.
    if not self._upstream:
      dash_q = '-q' if not self._verbose else ''
      cros_lib.Info('Checking out cros/gentoo at %s as upstream reference.' %
                    self._upstream_repo)
      self._RunGit(self._upstream_repo, 'checkout %s cros/gentoo' % dash_q)

    # An empty directory is needed to trick equery later.
    self._emptydir = tempfile.mkdtemp()

  def RunCompleted(self):
    """Undo any checkout of upstream gentoo that was done."""
    if not self._upstream:
      dash_q = '-q' if not self._verbose else ''
      cros_lib.Info('Undoing checkout of cros/gentoo at %s.' %
                    self._upstream_repo)
      self._RunGit(self._upstream_repo, 'checkout %s cros/master' % dash_q)

    os.rmdir(self._emptydir)

  def CommitIsStaged(self):
    """Return True if upgrades are staged and ready for a commit."""
    return bool(self._upgrade_cnt)

  def Commit(self):
    """Commit whatever has been prepared in the stable repo."""
    # Trying to create commit message body lines that look like these:
    # Upgraded foo/bar-1.2.3 to version 1.2.4
    # Upgraded foo/bar-1.2.3 to version 1.2.4 (arm) AND version 1.2.4-r1 (x86)
    # Upgraded foo/bar-1.2.3 to version 1.2.4 (but emerge fails)

    commit_lines = [] # Lines for the body of the commit message
    key_lines = []    # Lines needed in package.keywords

    # Assemble hash of COL_UPGRADED column names by arch.
    upgraded_cols = {}
    for arch in self._master_archs:
      tmp_col = utable.UpgradeTable.COL_UPGRADED
      col = utable.UpgradeTable.GetColumnName(tmp_col, arch)
      upgraded_cols[arch] = col

    table = self._master_table
    for row in table:
      pkg = row[table.COL_PACKAGE]
      pkg_commit_line = None
      pkg_keys = {} # key=cpv, value=set of arches that need keyword overwrite

      # First determine how many unique upgraded versions there are.
      upgraded_vers = set()
      for arch in self._master_archs:
        upgraded_ver = row[upgraded_cols[arch]]
        if upgraded_ver:
          upgraded_vers.add(upgraded_ver)

          # Is this upgraded version unstable for this arch?  If so, save
          # arch under upgraded_cpv as one that will need a package.keywords
          # entry.  Check by comparing against stable upstream version.
          tmp_col = utable.UpgradeTable.COL_STABLE_UPSTREAM_VER
          upstream_stable_col = utable.UpgradeTable.GetColumnName(tmp_col, arch)
          upstream_stable_ver = row[upstream_stable_col]
          if upgraded_ver != upstream_stable_ver:
            upgraded_cpv = pkg + '-' + upgraded_ver
            cpv_key = pkg_keys.get(upgraded_cpv, set())
            cpv_key.add(arch)
            pkg_keys[upgraded_cpv] = cpv_key

      if len(upgraded_vers) == 1:
        # Upgrade is the same across all archs.
        upgraded_ver = upgraded_vers.pop()
        arch_str = ', '.join(sorted(self._master_archs))
        pkg_commit_line = ('Upgraded %s to version %s on %s' %
                           (pkg, upgraded_ver, arch_str))
      elif len(upgraded_vers) > 1:
        # Iterate again, and specify arch for each upgraded version.
        tokens = []
        for arch in self._master_archs:
          upgraded_ver = row[upgraded_cols[arch]]
          if upgraded_ver:
            tokens.append('%s on %s' % (upgraded_ver, arch))
        pkg_commit_line = ('Upgraded %s to versions %s' %
                           (pkg, ' AND '.join(tokens)))

      if pkg_commit_line:
        commit_lines.append(pkg_commit_line)
      if len(pkg_keys):
        for upgraded_cpv in pkg_keys:
          cpv_key = pkg_keys[upgraded_cpv]
          key_lines.append('=%s %s' %
                           (upgraded_cpv,
                            ' '.join(cpv_key)))

    if commit_lines:
      if self._amend:
        message = self._AmendCommitMessage(commit_lines)
        self._RunGit(self._stable_repo, "commit --amend -am '%s'" % message)
      else:
        message = self._CreateCommitMessage(commit_lines)
        self._RunGit(self._stable_repo, "commit -am '%s'" % message)

      cros_lib.Warning('Upgrade changes committed (see above),'
                       ' but message needs edit BY YOU:\n'
                       ' cd %s; git commit --amend; cd -' %
                       self._stable_repo)

      if key_lines:
        cros_lib.Warning('Because one or more unstable versions are involved'
                         ' you must add line(s) like the following to\n'
                         ' %s:\n%s\n'
                         'This is needed before testing, and should be pushed'
                         ' in a separate changelist BEFORE the\n'
                         'the changes in portage-stable.' %
                         (self._GetPkgKeywordsFile(),
                          '\n'.join(key_lines)))
      cros_lib.Info('If you wish to undo these changes instead:\n'
                    ' cd %s; git reset --hard HEAD^; cd -' %
                    self._stable_repo)

  def PreRunChecks(self):
    """Run any board-independent validation checks before Run is called."""
    # Upfront check(s) if upgrade is requested.
    if self._upgrade or self._upgrade_deep:
      # Stable source must be on branch.
      self._CheckStableRepoOnBranch()

  def RunBoard(self, board):
    """Runs the upgrader based on the supplied options and arguments.

    Currently just lists all package dependencies in pre-order along with
    potential upgrades."""
    # Preserve status report for entire stable repo (output of 'git status -s').
    self._SaveStatusOnStableRepo()

    self._porttree = None
    self._deps_graph = None

    self._curr_board = board
    self._curr_arch = Upgrader._FindBoardArch(board)
    upgrade_mode = self._upgrade or self._upgrade_deep
    self._curr_table = utable.UpgradeTable(self._curr_arch,
                                           upgrade=upgrade_mode,
                                           name=board)

    cpvlist = self._GetCurrentVersions()
    infolist = self._GetInfoListWithOverlays(cpvlist)
    self._UpgradePackages(infolist)

    # Merge tables together after each run.
    self._master_cnt += 1
    self._master_archs.add(self._curr_arch)
    if self._master_table:
      tables = [self._master_table, self._curr_table]
      self._master_table = mps.MergeTables(tables)
    else:
      self._master_table = self._curr_table
      self._master_table._arch = None

  def WriteTableFiles(self, csv=None, html=None):
    """Write |table| to |csv| and/or |html| files, if requested."""

    # Sort the table by package name, then slot
    def PkgSlotSort(row):
      return (row[self._master_table.COL_PACKAGE],
              row[self._master_table.COL_SLOT])
    self._master_table.Sort(PkgSlotSort)

    if csv:
      filehandle = open(csv, 'w')
      # TODO(mtennant): change to cros_lib.Info
      print "Writing package status as csv to %s" % csv
      self._master_table.WriteCSV(filehandle)
      filehandle.close()
    if html:
      filehandle = open(html, 'w')
      # TODO(mtennant): change to cros_lib.Info
      print "Writing package status as html to %s" % html
      self._master_table.WriteHTML(filehandle)
      filehandle.close()

def _BoardIsSetUp(board):
  """Return true if |board| has been setup."""
  return os.path.isdir('/build/%s' % board)

def main():
  """Main function."""
  usage = 'Usage: %prog [options] packages...'
  epilog = ('\n'
            'There are essentially two "modes": status report mode and '
            'upgrade mode.\nStatus report mode is the default; upgrade '
            'mode is enabled by either --upgrade or --upgrade-deep.\n'
            '\n'
            'Status report mode will report on the status of the specified '
            'packages relative to upstream,\nwithout making any changes.'
            'In this mode, the specified packages are often high-level\n'
            'targets such as "chromeos" or "chromeos-dev". '
            'The --to-csv option is often used in this mode.\n'
            'The --unstable-ok option in this mode will make '
            'the upstream comparison consider unstable versions, also.\n'
            '\n'
            'Upgrade mode will attempt to upgrade the specified '
            'packages to the latest upstream version.\nUnlike with --upgrade, '
            'if --upgrade-deep is specified, then the package dependencies\n'
            'will also be upgraded.\n'
            '\n'
            'Status report mode examples:\n'
            '> cros_portage_upgrade --board=tegra2_aebl '
            '--to-csv=cros-aebl.csv chromeos\n'
            '> cros_portage_upgrade --unstable-ok --board=x86-mario '
            '--to-csv=cros_test-mario chromeos chromeos-dev chromeos-test\n'
            'Upgrade mode examples:\n'
            '> cros_portage_upgrade --board=tegra2_aebl '
            '--upgrade dbus\n'
            '> cros_portage_upgrade --unstable-ok --board=x86-mario '
            '--upgrade-deep gdata\n'
            )

  class MyOptParser(optparse.OptionParser):
    """Override default epilog formatter, which strips newlines."""
    def format_epilog(self, formatter):
      return self.epilog

  parser = MyOptParser(usage=usage, epilog=epilog)
  parser.add_option('--amend', dest='amend', action='store_true',
                    default=False,
                    help="Amend existing commit when doing upgrade.")
  parser.add_option('--board', dest='board', type='string', action='store',
                    default=None, help="Target board")
  parser.add_option('--host', dest='host', action='store_true',
                    default=False,
                    help="Host target pseudo-board")
  parser.add_option('--rdeps', dest='rdeps', action='store_true',
                    default=False,
                    help="Use runtime dependencies only")
  parser.add_option('--srcroot', dest='srcroot', type='string', action='store',
                    default='%s/trunk/src' % os.environ['HOME'],
                    help="Path to root src directory [default: '%default']")
  parser.add_option('--to-csv', dest='csv_file', type='string', action='store',
                    default=None, help="File to write csv-formatted results to")
  parser.add_option('--to-html', dest='html_file',
                    type='string', action='store', default=None,
                    help="File to write html-formatted results to")
  parser.add_option('--upgrade', dest='upgrade',
                    action='store_true', default=False,
                    help="Upgrade target package(s) only")
  parser.add_option('--upgrade-deep', dest='upgrade_deep', action='store_true',
                    default=False,
                    help="Upgrade target package(s) and all dependencies")
  parser.add_option('--upstream', dest='upstream', type='string',
                    action='store', default=None,
                    help="Latest upstream repo location [default: '%default']")
  parser.add_option('--unstable-ok', dest='unstable_ok', action='store_true',
                    default=False,
                    help="Use latest upstream ebuild, stable or not")
  parser.add_option('--verbose', dest='verbose', action='store_true',
                    default=False,
                    help="Enable verbose output (for debugging)")

  (options, args) = parser.parse_args()

  if (options.verbose): logging.basicConfig(level=logging.DEBUG)

  if not options.board and not options.host:
    parser.print_help()
    cros_lib.Die('Board (or host) is required.')

  if not args:
    parser.print_help()
    cros_lib.Die('No packages provided.')

  # The --upgrade and --upgrade-deep options are mutually exclusive.
  if options.upgrade_deep and options.upgrade:
    parser.print_help()
    cros_lib.Die('The --upgrade and --upgrade-deep options ' +
                 'are mutually exclusive.')

  # If upstream portage is provided, verify that it is a valid directory.
  if options.upstream and not os.path.isdir(options.upstream):
    parser.print_help()
    cros_lib.Die('Argument to --upstream must be a valid directory.')

  # If --to-csv or --to-html given, verify file can be opened for write.
  if options.csv_file and not os.access(options.csv_file, os.W_OK):
    parser.print_help()
    cros_lib.Die('Unable to open %s for writing.' % options.csv_file)
  if options.html_file and not os.access(options.html_file, os.W_OK):
    parser.print_help()
    cros_lib.Die('Unable to open %s for writing.' % options.html_file)

  upgrader = Upgrader(options, args)
  upgrader.PreRunChecks()

  # Automatically handle board 'host' as 'amd64-host'.
  boards = []
  if options.board:
    boards = options.board.split(':')
    boards = [Upgrader.HOST_BOARD if b == 'host' else b for b in boards]
  if options.host and Upgrader.HOST_BOARD not in boards:
    boards.append(Upgrader.HOST_BOARD)

  # Check that all boards have been setup first.
  for board in boards:
    if (board != Upgrader.HOST_BOARD and not _BoardIsSetUp(board)):
      parser.print_help()
      cros_lib.Die('You must setup the %s board first.' % board)

  try:
    upgrader.PrepareToRun()

    for board in boards:
      cros_lib.Info('Running with board %s' % board)
      upgrader.RunBoard(board)

  finally:
    upgrader.RunCompleted()

  if upgrader.CommitIsStaged():
    upgrader.Commit()

  # TODO(mtennant): Move stdout output to here, rather than as-we-go.  That
  # way it won't come out for each board.  Base it on contents of final table.
  # Make verbose-dependent?

  upgrader.WriteTableFiles(csv=options.csv_file, html=options.html_file)


if __name__ == '__main__':
  main()
