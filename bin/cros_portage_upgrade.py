#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Perform various tasks related to updating Portage packages."""

import filecmp
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
import chromite.lib.operation as operation
import chromite.lib.upgrade_table as utable
import merge_package_status as mps

oper = operation.Operation('cros_portage_upgrade')
oper.verbose = True # Without verbose Info messages don't show up.

NOT_APPLICABLE = 'N/A'
WORLD_TARGET = 'world'
UPGRADED = 'Upgraded'

# Files we do not include in our upgrades by convention.
BLACKLISTED_FILES = set(['Manifest', 'ChangeLog', 'metadata.xml'])

# TODO(mtennant): Use this class to replace the 'info' dictionary used
# throughout. In the meantime, it simply serves as documentation for
# the values in that dictionary.
class PInfo(object):
  """Class to accumulate package info during upgrade process.

  This class is basically a formalized dictionary."""

  __slots__ = [
      'category',            # Package category only
      # TODO(mtennant): Rename 'cpv' to 'curr_cpv' or similar.
      'cpv',                 # Current full cpv (revision included)
      'cpv_cmp_upstream',    # 0 = current, >0 = outdated, <0 = futuristic
      'latest_upstream_cpv', # Latest (non-stable ok) upstream cpv
      'overlay',             # Overlay package currently in
      'package',             # category/package_name
      'package_name',        # The 'p' in 'cpv'
      'package_ver',         # The 'pv' in 'cpv'
      'slot',                # Current package slot
      'stable_upstream_cpv', # Latest stable upstream cpv
      'state',               # One of utable.UpgradeTable.STATE_*
      'upgraded_cpv',        # If upgraded, it is to this cpv
      'upgraded_stable',     # Boolean. If upgraded_cpv, indicates if stable.
      'upgraded_unmasked',   # Boolean. If upgraded_cpv, indicates if unmasked.
      'upstream_cpv',        # latest/stable upstream cpv according to request
      'user_arg',            # Original user arg for this pkg, if applicable
      'version_rev',         # Just revision (e.g. 'r1').  '' if no revision
      ]

class Upgrader(object):
  """A class to perform various tasks related to updating Portage packages."""

  PORTAGE_GIT_URL = ('ssh://gerrit.chromium.org:29418/'
                     'chromiumos/overlays/portage.git')
  ORIGIN_GENTOO = 'origin/gentoo'
  UPSTREAM_TMP_REPO = '/tmp/cros_portage_upgrade-gentoo-portage'
  UPSTREAM_OVERLAY_NAME = 'portage'
  STABLE_OVERLAY_NAME = 'portage-stable'
  CROS_OVERLAY_NAME = 'chromiumos-overlay'
  HOST_BOARD = 'amd64-host'
  OPT_SLOTS = ['amend', 'csv_file', 'force', 'no_upstream_cache', 'rdeps',
               'upgrade', 'upgrade_deep', 'upstream', 'unstable_ok', 'verbose']

  EQUERY_CMD = 'equery'
  EMERGE_CMD = 'emerge'
  PORTAGEQ_CMD = 'portageq'
  BOARD_CMDS = set([EQUERY_CMD, EMERGE_CMD, PORTAGEQ_CMD])

  __slots__ = ['_amend',        # Boolean to use --amend with upgrade commit
               '_args',         # Commandline arguments (all portage targets)
               '_curr_arch',    # Architecture for current board run
               '_curr_board',   # Board for current board run
               '_curr_table',   # Package status for current board run
               '_cros_overlay', # Path to chromiumos-overlay repo
               '_csv_file',     # File path for writing csv output
               '_deps_graph',   # Dependency graph from portage
               '_force',        # Force upgrade even when version already exists
               '_missing_eclass_re',# Regexp for missing eclass in equery
               '_outdated_eclass_re',# Regexp for outdated eclass in equery
               '_emptydir',     # Path to temporary empty directory
               '_equery_regexp',# Regexp mask bits from 'equery list' output
               '_master_archs', # Set. Archs of tables merged into master_table
               '_master_cnt',   # Number of tables merged into master_table
               '_master_table', # Merged table from all board runs
               '_no_upstream_cache', # Boolean.  Delete upstream cache when done
               '_porttree',     # Reference to portage porttree object
               '_rdeps',        # Boolean, if True pass --root-deps=rdeps
               '_stable_repo',  # Path to portage-stable
               '_stable_repo_stashed', # True if portage-stable has a git stash
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
      self._upstream_repo = self.UPSTREAM_TMP_REPO
    self._cros_overlay = os.path.join(options.srcroot, 'third_party',
                                      self.CROS_OVERLAY_NAME)

    # Save options needed later.
    for opt in self.OPT_SLOTS:
      setattr(self, '_' + opt, getattr(options, opt, None))

    self._porttree = None
    self._emptydir = None
    self._deps_graph = None

    # Pre-compiled regexps for speed.
    self._equery_regexp = re.compile(r'^\[...\]\s*\[(.)(.)\]\s+\S+$')
    self._missing_eclass_re = re.compile(r'(\S+\.eclass) could not be '
                                         'found by inherit')
    self._outdated_eclass_re = re.compile(r'Call stack:\n'
                                          '(?:.*?\s+\S+,\sline.*?\n)*'
                                          '.*?\s+(\S+\.eclass),\s+line')

  def _IsInUpgradeMode(self):
    """Return True if running in upgrade mode."""
    return self._upgrade or self._upgrade_deep

  # TODO(mtennant): Make this smart enough to recommend the
  # targets/chromeos/package.keywords file when applicable.
  def _GetPkgKeywordsFile(self):
    """Return the path to the package.keywords file in chromiumos-overlay."""
    return '%s/profiles/default/linux/package.keywords' % self._cros_overlay

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
      raise RuntimeError("Unable to run 'git status -s' in %s:\n%s" %
                         (self._stable_repo, result.output))

    self._stable_repo_stashed = False

  def _CheckStableRepoOnBranch(self):
    """Raise exception if |self._stable_repo| is not on a branch now."""
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
      return bool('user_arg' in info)

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
    """Returns standard cmp result between |cpv1| and |cpv2|.

    If one cpv is None then the other is greater.
    """
    if cpv1 is None and cpv2 is None:
      return 0
    if cpv1 is None:
      return -1
    if cpv2 is None:
      return 1
    return portage.versions.pkgcmp(portage.versions.pkgsplit(cpv1),
                                   portage.versions.pkgsplit(cpv2))

  @staticmethod
  def _GetCatPkgFromCpv(cpv):
    """Returns category/package_name from a full |cpv|.

    If |cpv| is incomplete, may return only the package_name.

    If package_name cannot be determined, return None.
    """
    if not cpv:
      return None

    # Result is None or (cat, pn, version, rev)
    result = portage.versions.catpkgsplit(cpv)
    if result:
      # This appears to be a quirk of portage? Category string == 'null'.
      if result[0] is None or result[0] == 'null':
        return result[1]
      return '%s/%s' % (result[0], result[1])

    return None

  @staticmethod
  def _GetVerRevFromCpv(cpv):
    """Returns just the version-revision string from a full |cpv|."""
    if not cpv:
      return None

    # Result is None or (cat, pn, version, rev)
    result = portage.versions.catpkgsplit(cpv)
    if result:
      (version, rev) = result[2:4]
      if rev != 'r0':
        return '%s-%s' % (version, rev)
      else:
        return version

    return None

  def _RunGit(self, repo, command, redirect_stdout=False,
              combine_stdout_stderr=False):
    """Runs |command| in the git |repo|.

    This leverages the cros_build_lib.RunCommand function.  The
    |redirect_stdout| and |combine_stdout_stderr| arguments are
    passed to that function.

    Returns a Result object as documented by cros_build_lib.RunCommand.
    Most usefully, the result object has a .output attribute containing
    the output from the command (if |redirect_stdout| was True).
    """
    cmdline = ['/bin/sh', '-c', 'cd %s && git -c core.pager=" " %s' %
               (repo, command)]
    result = cros_lib.RunCommand(cmdline, exit_code=True,
                                 print_cmd=self._verbose,
                                 redirect_stdout=redirect_stdout,
                                 combine_stdout_stderr=combine_stdout_stderr)
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

  def _FindUpstreamCPV(self, pkg, unstable_ok=False):
    """Returns latest cpv in |_upstream_repo| that matches |pkg|.

    The |pkg| argument can specify as much or as little of the full CPV
    syntax as desired, exactly as accepted by the Portage 'equery' command.
    To find whether an exact version exists upstream specify the full
    CPV.  To find the latest version specify just the category and package
    name.

    Results are filtered by architecture keyword using |self._curr_arch|.
    By default, only ebuilds stable on that arch will be accepted.  To
    accept unstable ebuilds, set |unstable_ok| to True.

    Returns upstream cpv, if found.
    """
    envvars = self._GenPortageEnvvars(self._curr_arch, unstable_ok,
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

  def _GetBoardCmd(self, cmd):
    """Return the board-specific version of |cmd|, if applicable."""
    if cmd in self.BOARD_CMDS:
      # Host "board" is a special case.
      if self._curr_board != self.HOST_BOARD:
        return '%s-%s' % (cmd, self._curr_board)

    return cmd

  def _AreEmergeable(self, cpvlist, unstable_ok):
    """Indicate whether cpvs in |cpvlist| can be emerged on current board.

    This essentially runs emerge with the --pretend option to verify
    that all dependencies for these package versions are satisfied.

    The |unstable_ok| argument determines whether an unstable version
    is acceptable.

    Return tuple with two elements:
    [0] True if |cpvlist| can be emerged.
    [1] Output from the emerge command.
    """
    envvars = self._GenPortageEnvvars(self._curr_arch, unstable_ok)
    emerge = self._GetBoardCmd(self.EMERGE_CMD)
    cmd = [emerge, '-p'] + ['=' + cpv for cpv in cpvlist]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )

    return (result.returncode == 0, ' '.join(cmd), result.output)

  def _FindCurrentCPV(self, pkg):
    """Returns current cpv on |_curr_board| that matches |pkg|, or None."""
    envvars = self._GenPortageEnvvars(self._curr_arch, False)

    equery = self._GetBoardCmd(self.EQUERY_CMD)
    cmd = [equery, 'which', pkg]
    cmd_result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                     extra_env=envvars, print_cmd=False,
                                     redirect_stdout=True,
                                     combine_stdout_stderr=True,
                                     )

    if cmd_result.returncode == 0:
      ebuild_path = cmd_result.output.strip()
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      return os.path.join(cat, pv)
    else:
      return None

  def _GetMaskBits(self, cpv):
    """Return two-Boolean tuple (unmasked, stable) for |cpv| on current arch."""

    envvars = self._GenPortageEnvvars(self._curr_arch, False)

    equery = self._GetBoardCmd('equery')
    cmd = [equery, '--no-pipe', 'list', '-op', cpv]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )
    output = result.output.strip()

    # Expect output like one of these cases (~ == unstable, M == masked):
    # [-P-] [ ~] sys-fs/fuse-2.7.3:0
    # [-P-] [  ] sys-fs/fuse-2.7.3:0
    # [-P-] [M ] sys-fs/fuse-2.7.3:0
    # [-P-] [M~] sys-fs/fuse-2.7.3:0
    for line in output.split('\n'):
      match = self._equery_regexp.search(line)
      if match:
        return ('M' != match.group(1), '~' != match.group(2))

    raise RuntimeError("Unable to determine whether %s is stable from equery:\n"
                       " %s\noutput:\n %s" % (cpv, ' '.join(cmd), output))

  def _VerifyEbuildOverlay(self, cpv, expected_overlay,
                           stable_only, was_overwrite):
    """Raises exception if ebuild for |cpv| is not from |expected_overlay|.

    Essentially, this verifies that the upgraded ebuild in portage-stable
    is indeed the one being picked up, rather than some other ebuild with
    the same version in another overlay.  Unless |was_overwrite| (see below).

    If |was_overwrite| then this upgrade was an overwrite of an existing
    package version (via --force) and it is possible the previous package
    is still in another overlay (e.g. chromiumos-overlay).  In this case,
    the user should get rid of the other version first.
    """
    # Further explanation: this check should always pass, but might not
    # if the copy/upgrade from upstream did not work.  This is just a
    # sanity check.
    envvars = self._GenPortageEnvvars(self._curr_arch, not stable_only)

    equery = self._GetBoardCmd(self.EQUERY_CMD)
    cmd = [equery, 'which', '--include-masked', cpv]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )
    ebuild_path = result.output.strip()
    (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
    if overlay != expected_overlay:
      if was_overwrite:
        raise RuntimeError('Upgraded ebuild for %s is not visible because'
                           ' existing ebuild in %s overlay takes precedence\n'
                           'Please remove that ebuild before continuing.' %
                           (cpv, overlay))
      else:
        raise RuntimeError('Upgraded ebuild for %s is not coming from %s:\n'
                           ' %s\n'
                           'Please show this error to the build team.' %
                           (cpv, expected_overlay, ebuild_path))

  def _IdentifyNeededEclass(self, cpv):
    """Return eclass that must be upgraded for this |cpv|."""
    # Try to detect two cases:
    # 1) The upgraded package uses an eclass not in local source, yet.
    # 2) The upgraded package needs one or more eclasses to also be upgraded.

    # Use the output of 'equery which'.
    # If a needed eclass cannot be found, then the output will have lines like:
    # * ERROR: app-admin/eselect-1.2.15 failed (depend phase):
    # *   bash-completion-r1.eclass could not be found by inherit()

    # If a needed eclass must be upgraded, the output might have the eclass
    # in the call stack (... used for long paths):
    # * Call stack:
    # *            ebuild.sh, line 2047:  Called source '.../vim-7.3.189.ebuild'
    # *   vim-7.3.189.ebuild, line    7:  Called inherit 'vim'
    # *            ebuild.sh, line 1410:  Called qa_source '.../vim.eclass'
    # *            ebuild.sh, line   43:  Called source '.../vim.eclass'
    # *           vim.eclass, line   40:  Called die
    # * The specific snippet of code:
    # *       die "Unknown EAPI ${EAPI}"

    envvars = self._GenPortageEnvvars(self._curr_arch, unstable_ok=True)

    equery = self._GetBoardCmd(self.EQUERY_CMD)
    cmd = [equery, '--no-pipe', 'which', cpv]
    result = cros_lib.RunCommand(cmd, exit_code=True, error_ok=True,
                                 extra_env=envvars, print_cmd=False,
                                 redirect_stdout=True,
                                 combine_stdout_stderr=True,
                                 )

    if result.returncode != 0:
      output = result.output.strip()

      # _missing_eclass_re works line by line.
      for line in output.split('\n'):
        match = self._missing_eclass_re.search(line)
        if match:
          eclass = match.group(1)
          oper.Info("Determined that %s requires %s" % (cpv, eclass))
          return eclass

      # _outdated_eclass_re works on the entire output at once.
      match = self._outdated_eclass_re.search(output)
      if match:
        eclass = match.group(1)
        oper.Info("Making educated guess that %s requires update of %s" %
                  (cpv, eclass))
        return eclass

    return None

  def _GiveMaskedError(self, upgraded_cpv, emerge_output):
    """Print error saying that |upgraded_cpv| is masked off.

    See if hint found in |emerge_output| to improve error emssage.
    """

    # Expecting emerge_output to have lines like this:
    #  The following mask changes are necessary to proceed:
    # #required by ... =somecategory/somepackage (some reason)
    # # /home/mtennant/trunk/src/third_party/chromiumos-overlay/profiles\
    # /targets/chromeos/package.mask:
    # >=upgraded_cp
    package_mask = None

    upgraded_cp = Upgrader._GetCatPkgFromCpv(upgraded_cpv)
    regexp = re.compile(r'#\s*required by.+=\S+.*\n'
                        '#\s*(\S+/package\.mask):\s*\n'
                        '[<>=]+%s' % upgraded_cp)

    match = regexp.search(emerge_output)
    if match:
      package_mask = match.group(1)

    if package_mask:
      oper.Error("\nUpgraded package '%s' appears to be masked by a line in\n"
                 "'%s'\n"
                 "Full emerge output is above. Address mask issue, "
                 "then run this again." %
                 (upgraded_cpv, package_mask))
    else:
      oper.Error("\nUpgraded package '%s' is masked somehow (See full "
                 "emerge output above). Address that and then run this "
                 "again." % upgraded_cpv)

  def _PkgUpgradeStaged(self, upstream_cpv):
    """Return True if package upgrade is already staged."""
    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(upstream_cpv)
    ebuild = upstream_cpv.replace(cat + '/', '') + '.ebuild'
    ebuild_relative_path = os.path.join(cat, pkgname, ebuild)

    status = self._stable_repo_status.get(ebuild_relative_path, None)
    if status and 'A' == status:
      return True

    return False

  def _AnyChangesStaged(self):
    """Return True if any local changes are staged in stable repo."""
    # Don't count files with '??' status - they aren't staged.
    files = [f for (f, s) in self._stable_repo_status.items() if s != '??']
    return bool(len(files))

  def _StashChanges(self):
    """Run 'git stash save' on stable repo."""
    # Only one level of stashing expected/supported.
    self._RunGit(self._stable_repo, 'stash save',
                 redirect_stdout=True, combine_stdout_stderr=True)
    self._stable_repo_stashed = True

  def _UnstashAnyChanges(self):
    """Unstash any changes in stable repo."""
    # Only one level of stashing expected/supported.
    if self._stable_repo_stashed:
      self._RunGit(self._stable_repo, 'stash pop',
                   redirect_stdout=True, combine_stdout_stderr=True)
      self._stable_repo_stashed = False

  def _DropAnyStashedChanges(self):
    """Drop any stashed changes in stable repo."""
    # Only one level of stashing expected/supported.
    if self._stable_repo_stashed:
      self._RunGit(self._stable_repo, 'stash drop',
                   redirect_stdout=True, combine_stdout_stderr=True)
      self._stable_repo_stashed = False

  def _CopyUpstreamPackage(self, upstream_cpv):
    """Upgrades package in |upstream_cpv| to the version in |upstream_cpv|.

    Returns:
      The upstream_cpv if the package was upgraded, None otherwise.
    """
    oper.Info('Copying %s from upstream.' % upstream_cpv)

    (cat, pkgname, version, rev) = portage.versions.catpkgsplit(upstream_cpv)
    ebuild = upstream_cpv.replace(cat + '/', '') + '.ebuild'
    catpkgsubdir = os.path.join(cat, pkgname)
    pkgdir = os.path.join(self._stable_repo, catpkgsubdir)
    upstream_pkgdir = os.path.join(self._upstream_repo, cat, pkgname)

    # Fail early if upstream_cpv ebuild is not found
    upstream_ebuild_path = os.path.join(upstream_pkgdir, ebuild)
    if not os.path.exists(upstream_ebuild_path):
      # Note: this should only be possible during unit tests.
      raise RuntimeError("Cannot find upstream ebuild at '%s'" %
                         upstream_ebuild_path)

    # If pkgdir already exists, remove everything in it.
    if os.path.exists(pkgdir):
      items = os.listdir(pkgdir)
      for item in items:
        src = os.path.join(upstream_pkgdir, item)
        self._RunGit(pkgdir, 'rm -rf ' + item, redirect_stdout=True)
    else:
      os.makedirs(pkgdir)

    # Grab all non-blacklisted, non-ebuilds from upstream plus the specific
    # ebuild requested.
    # TODO: Selectively exclude files under the 'files' directory that clearly
    # apply to other package versions.  For example, 'files/foo-1.2.3-bar.patch'
    # when upgrading to foo-3.2.1.  Must do this carefully.
    items = os.listdir(upstream_pkgdir)
    for item in items:
      if os.path.basename(item) not in BLACKLISTED_FILES:
        if not item.endswith('.ebuild') or item == ebuild:
          src = os.path.join(upstream_pkgdir, item)
          dst = os.path.join(pkgdir, item)
          if os.path.isdir(src):
            shutil.copytree(src, dst, symlinks=True)
          else:
            shutil.copy2(src, dst)

    self._RunGit(self._stable_repo, 'add ' + catpkgsubdir)

    # Now copy any eclasses that this package requires.
    eclass = self._IdentifyNeededEclass(upstream_cpv)
    while eclass and self._CopyUpstreamEclass(eclass):
      eclass = self._IdentifyNeededEclass(upstream_cpv)

    return upstream_cpv

  def _CopyUpstreamEclass(self, eclass):
    """Upgrades eclass in |eclass| to upstream copy.

    Does not do the copy if the eclass already exists locally and
    is identical to the upstream version.

    Returns True if the copy was done."""
    eclass_subpath = os.path.join('eclass', eclass)
    upstream_path = os.path.join(self._upstream_repo, eclass_subpath)
    local_path = os.path.join(self._stable_repo, eclass_subpath)

    if os.path.exists(upstream_path):
      if os.path.exists(local_path) and filecmp.cmp(upstream_path, local_path):
        return False
      else:
        oper.Info('Copying %s from upstream.' % eclass)
        if not os.path.exists(os.path.dirname(local_path)):
          os.makedirs(os.path.dirname(local_path))
        shutil.copy2(upstream_path, local_path)
        self._RunGit(self._stable_repo, 'add %s' % eclass_subpath)
        return True

    raise RuntimeError("Cannot find upstream '%s'.  Looked at '%s'" %
                       (eclass, upstream_path))

  def _GetPackageUpgradeState(self, info):
    """Return state value for package in |info|."""
    # See whether this specific cpv exists upstream.
    cpv = info['cpv']
    cpv_exists_upstream = bool(self._FindUpstreamCPV(cpv, unstable_ok=True))

    # The value in info['cpv_cmp_upstream'] represents a comparison of cpv
    # version and the upstream version, where:
    # 0 = current, >0 = outdated, <0 = futuristic!

    # Convention is that anything not in portage overlay has been altered.
    overlay = info['overlay']
    locally_patched = (overlay != NOT_APPLICABLE and
                       overlay != self.UPSTREAM_OVERLAY_NAME and
                       overlay != self.STABLE_OVERLAY_NAME)
    locally_duplicated = locally_patched and cpv_exists_upstream

    # Gather status details for this package
    if info['cpv_cmp_upstream'] is None:
      # No upstream cpv to compare to (although this might include a
      # a restriction to only stable upstream versions).  This is concerning
      # if the package is coming from 'portage' or 'portage-stable' overlays.
      if locally_patched and info['latest_upstream_cpv'] is None:
        state = utable.UpgradeTable.STATE_LOCAL_ONLY
      else:
        state = utable.UpgradeTable.STATE_UNKNOWN
    elif info['cpv_cmp_upstream'] > 0:
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
      action_stat = ' (UPGRADED)'
    else:
      action_stat = ''

    up_stat = {utable.UpgradeTable.STATE_UNKNOWN: ' no package found upstream!',
               utable.UpgradeTable.STATE_LOCAL_ONLY: ' (exists locally only)',
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
    upgraded_cpv = info['upgraded_cpv']

    upgraded_ver = ''
    if upgraded_cpv:
      upgraded_ver = Upgrader._GetVerRevFromCpv(upgraded_cpv)

      # "~" Prefix means the upgraded version is not stable on this arch.
      if not info['upgraded_stable']:
        upgraded_ver = '~' + upgraded_ver

    # Assemble 'depends on' and 'required by' strings.
    depsstr = NOT_APPLICABLE
    usedstr = NOT_APPLICABLE
    if cpv and self._deps_graph:
      deps_entry = self._deps_graph[cpv]
      depslist = sorted(deps_entry['needs'].keys()) # dependencies
      depsstr = ' '.join(depslist)
      usedset = deps_entry['provides'] # used by
      usedlist = sorted([p for p in usedset])
      usedstr = ' '.join(usedlist)

    stable_up_ver = Upgrader._GetVerRevFromCpv(info['stable_upstream_cpv'])
    if not stable_up_ver:
      stable_up_ver = NOT_APPLICABLE
    latest_up_ver = Upgrader._GetVerRevFromCpv(info['latest_upstream_cpv'])
    if not latest_up_ver:
      latest_up_ver = NOT_APPLICABLE

    row = {self._curr_table.COL_PACKAGE: info['package'],
           self._curr_table.COL_SLOT: info['slot'],
           self._curr_table.COL_OVERLAY: info['overlay'],
           self._curr_table.COL_CURRENT_VER: info['version_rev'],
           self._curr_table.COL_STABLE_UPSTREAM_VER: stable_up_ver,
           self._curr_table.COL_LATEST_UPSTREAM_VER: latest_up_ver,
           self._curr_table.COL_STATE: info['state'],
           self._curr_table.COL_DEPENDS_ON: depsstr,
           self._curr_table.COL_USED_BY: usedstr,
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

    The |info| hash must have the following entries:
    package, category, package_name
    cpv must exist but can be None

    Regardless, the following entries in |info| dict are filled in:
    stable_upstream_cpv
    latest_upstream_cpv
    upstream_cpv (one of the above, depending on --stable-only option)
    upgrade_cpv (if upgrade performed)
    """
    cpv = info['cpv']
    catpkg = info['package']
    info['stable_upstream_cpv'] = self._FindUpstreamCPV(catpkg)
    info['latest_upstream_cpv'] = self._FindUpstreamCPV(catpkg,
                                                        unstable_ok=True)

    # The upstream version can be either latest stable or latest overall,
    # or specified explicitly by the user at the command line.  In the latter
    # case, 'upstream_cpv' will already be set.
    if not info['upstream_cpv']:
      if not self._unstable_ok:
        info['upstream_cpv'] = info['stable_upstream_cpv']
      else:
        info['upstream_cpv'] = info['latest_upstream_cpv']

    # Perform the actual upgrade, if requested.
    info['cpv_cmp_upstream'] = None
    info['upgraded_cpv'] = None
    if info['upstream_cpv']:
      # cpv_cmp_upstream values: 0 = current, >0 = outdated, <0 = futuristic!
      info['cpv_cmp_upstream'] = Upgrader._CmpCpv(info['upstream_cpv'], cpv)

      # Determine whether upgrade of this package is requested.
      if self._PkgUpgradeRequested(info):
        if self._PkgUpgradeStaged(info['upstream_cpv']):
          oper.Info('Determined that %s is already staged.' %
                    info['upstream_cpv'])
          info['upgraded_cpv'] = info['upstream_cpv']
        elif info['cpv_cmp_upstream'] > 0:
          info['upgraded_cpv'] = self._CopyUpstreamPackage(info['upstream_cpv'])
        elif info['cpv_cmp_upstream'] == 0:
          if self._force:
            oper.Info('Forcing upgrade of existing %s.' %
                      info['upstream_cpv'])
            upgraded_cpv = self._CopyUpstreamPackage(info['upstream_cpv'])
            info['upgraded_cpv'] = upgraded_cpv
          else:
            oper.Warning('Not upgrading %s; it already exists in source.\n'
                         'To force upgrade of this version specify --force.' %
                         info['upstream_cpv'])

    return bool(info['upgraded_cpv'])

  def _VerifyPackageUpgrade(self, info):
    """Verify that the upgraded package in |info| passes checks."""
    # Verify that upgraded package can be emerged and save results.
    (unmasked, stable) = self._GetMaskBits(info['upgraded_cpv'])
    info['upgraded_unmasked'] = unmasked
    info['upgraded_stable'] = stable

    self._VerifyEbuildOverlay(info['upgraded_cpv'], self.STABLE_OVERLAY_NAME,
                              info['upgraded_stable'],
                              info['cpv_cmp_upstream'] == 0)

  def _PackageReport(self, info):
    """Report on whatever was done with package in |info|."""

    info['state'] = self._GetPackageUpgradeState(info)

    if self._verbose:
      # Print a quick summary of package status.
      self._PrintPackageLine(info)

    # Add a row to status table for this package
    self._AppendPackageRow(info)

  def _ExtractUpgradedPkgs(self, upgrade_lines):
    """Extracts list of packages from standard commit |upgrade_lines|."""
    # Expecting message lines like this (return just package names):
    # Upgraded sys-libs/ncurses to version 5.7-r7 on amd64, arm, x86
    # Upgraded sys-apps/less to version 441 on amd64, arm
    # Upgraded sys-apps/less to version 442 on x86
    pkgs = set()
    regexp = re.compile(r'^%s\s+\S+/(\S+)\s' % UPGRADED)
    for line in upgrade_lines:
      match = regexp.search(line)
      if match:
        pkgs.add(match.group(1))

    return sorted(pkgs)

  def _CreateCommitMessage(self, upgrade_lines, remaining_lines=[]):
    """Create appropriate git commit message for upgrades in |upgrade_lines|."""
    message = None
    upgrade_pkgs = self._ExtractUpgradedPkgs(upgrade_lines)
    upgrade_count = len(upgrade_pkgs)
    upgrade_str = '\n'.join(upgrade_lines)
    if upgrade_count == 1:
      message = ('Upgraded the %s Portage package\n\n%s\n' %
                 (upgrade_pkgs[0], upgrade_str))
    elif upgrade_count < 6:
      message = ('Upgraded the %s Portage packages\n\n%s\n' %
                 (', '.join(upgrade_pkgs), upgrade_str))
    else:
      message = ('Upgraded the following %d Portage packages\n\n%s\n' %
                 (upgrade_count, upgrade_str))

    if remaining_lines:
      # Keep previous remaining lines verbatim.
      message += '\n%s\n' % '\n'.join(remaining_lines)
    else:
      # The space before <fill-in> (at least for TEST=) fails pre-submit check,
      # which is the intention here.
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

    remaining_lines = []
    if match:
      # Extract the upgrade_lines of last commit.  Everything after the
      # empty line is preserved verbatim.
      # Expecting message body like this:
      # Upgraded sys-libs/ncurses to version 5.7-r7 on amd64, arm, x86
      # Upgraded sys-apps/less to version 441 on amd64, arm, x86
      #
      # BUG=chromium-os:20923
      # TEST=trybot run of chromiumos-sdk
      body = match.group(1)

      before_break = True
      for line in body.split('\n'):
        if not before_break:
          remaining_lines.append(line)
        elif line:
          if re.search(r'^%s\s+' % UPGRADED, line):
            upgrade_lines.append(line)
          else:
            # If the lines in the message body are not in the expected
            # format simply push them to the end of the new commit
            # message body, but left intact.
            oper.Warning("It looks like the existing commit message "
                         "that you are amending was not generated by\n"
                         "this utility.  Appending previous commit "
                         "message to newly generated message.")
            before_break = False
            remaining_lines.append(line)
        else:
          before_break = False

    return self._CreateCommitMessage(upgrade_lines, remaining_lines)

  def _GiveEmergeResults(self, infolist):
    """Summarize emerge checks, raise RuntimeError if there is a problem."""

    upgraded_infos = [info for info in infolist if info['upgraded_cpv']]
    upgraded_cpvs = [info['upgraded_cpv'] for info in upgraded_infos]
    masked_cpvs = set([info['upgraded_cpv'] for info in upgraded_infos
                       if not info['upgraded_unmasked']])

    (ok, cmd, output) = self._AreEmergeable(upgraded_cpvs, self._unstable_ok)

    if masked_cpvs:
      # If any of the upgraded_cpvs are masked, then emerge should have
      # failed.  Give a helpful message.  If it didn't fail then panic.
      if ok:
        oper.Error("\nEmerge passed for masked package(s)!  Something "
                   "fishy here. Emerge output follows:")
        print output
        raise RuntimeError("Show this to the build team.")

      else:
        oper.Error("\nEmerge output for '%s' on %s follows:" %
                   (cmd, self._curr_arch))
        print output
        for masked_cpv in masked_cpvs:
          self._GiveMaskedError(masked_cpv, output)
        raise RuntimeError("\nOne or more upgraded packages are masked "
                           "(see above).")

    if ok:
      oper.Info("Confirmed that all upgraded packages can be emerged "
                "on %s after upgrade." % self._curr_board)
    else:
      oper.Error("Packages cannot be emerged after upgrade.  The output "
                 "of '%s' follows:" % cmd)
      print output
      raise RuntimeError("Failed to complete upgrades on %s (see above). "
                         "Address the emerge errors before continuing." %
                         self._curr_board)

  def _UpgradePackages(self, infolist):
    """Given a list of cpv info maps, adds the upstream cpv to the infos."""
    self._curr_table.Clear()

    try:
      upgrades_this_run = False
      for info in infolist:
        if self._UpgradePackage(info):
          self._upgrade_cnt += 1
          upgrades_this_run = True

      # The verification of upgrades needs to happen after upgrades are done.
      # The reason is that it cannot be guaranteed that infolist is ordered such
      # that dependencies are satisified after each individual upgrade, because
      # one or more of the packages may only exist upstream.
      for info in infolist:
        if info['upgraded_cpv']:
          self._VerifyPackageUpgrade(info)

        self._PackageReport(info)

      if upgrades_this_run:
        self._GiveEmergeResults(infolist)

      if self._IsInUpgradeMode():
        # If there were any ebuilds staged before running this script, then
        # make sure they were targeted in infolist.  If not, abort.
        self._CheckStagedUpgrades(infolist)
    except RuntimeError as ex:
      oper.Error(str(ex))

      raise RuntimeError('\nTo reset all changes in %s now:\n'
                         ' cd %s; git reset --hard; cd -' %
                         (self._stable_repo, self._stable_repo))
      # Allow the changes to stay staged so that the user can attempt to address
      # the issue (perhaps an edit to package.mask is required, or another
      # package must also be upgraded).

  def _CheckStagedUpgrades(self, infolist):
    """Raise RuntimeError if staged upgrades are not also in |infolist|."""
    # This deals with the situation where a previous upgrade run staged one or
    # more package upgrades, but did not commit them because it found an error
    # of some kind.  This is ok, as long as subsequent runs continue to request
    # an upgrade of that package again (presumably with the problem fixed).
    # However, if a subsequent run does not mention that package then it should
    # abort.  The user must reset those staged changes first.

    if self._stable_repo_status:
      err_msgs = []

      # Go over files with pre-existing git statuses.
      filelist = self._stable_repo_status.keys()
      ebuilds = [e for e in filelist if e.endswith('.ebuild')]
      for ebuild in ebuilds:
        (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild)
        cpv = '%s/%s' % (cat, pv)

        # Look for info with ['upgraded_cpv'] that matches
        matching_infos = [i for i in infolist if i['upgraded_cpv'] == cpv]
        if not matching_infos:
          err_msgs.append("Staged %s is not one of upgrade targets." % ebuild)

      if err_msgs:
        raise RuntimeError("%s\n"
                           "Add to upgrade targets or reset staged changes." %
                           '\n'.join(err_msgs))

  def _GenParallelEmergeArgv(self, args):
    """Creates an argv for parallel_emerge using current options and |args|."""
    argv = ['--emptytree', '--pretend']
    if self._curr_board and self._curr_board != self.HOST_BOARD:
      argv.append('--board=%s' % self._curr_board)
    if not self._verbose:
      argv.append('--quiet')
    if self._rdeps:
      argv.append('--root-deps=rdeps')
    argv.extend(args)

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

  def _CreateInfoFromCPV(self, cpv, cpv_key=None):
    """Return a basic info object created from |cpv|."""
    info = {}
    self._FillInfoFromCPV(info, cpv, cpv_key)
    return info

  def _FillInfoFromCPV(self, info, cpv, cpv_key=None):
    """Flesh out |info| from |cpv|."""
    pkg = Upgrader._GetCatPkgFromCpv(cpv)
    (cat, pn) = pkg.split('/')

    info['cpv'] = None
    info['upstream_cpv'] = None

    info['package'] = pkg
    info['package_name'] = pn
    info['category'] = cat

    if cpv_key:
      info[cpv_key] = cpv

  def _GetCurrentVersions(self, target_infolist):
    """Returns a list of pkg infos of the current package dependencies.

    The dependencies are taken from giving the 'package' values in each
    info of |target_infolist| to (parallel_)emerge.

    The returned list is ordered such that the dependencies of any mentioned
    package occur earlier in the list."""
    emerge_args = []
    for info in target_infolist:
      local_cpv = info['cpv']
      if local_cpv and local_cpv != WORLD_TARGET:
        emerge_args.append('=' + local_cpv)
      else:
        emerge_args.append(info['package'])
    argv = self._GenParallelEmergeArgv(emerge_args)

    deps = parallel_emerge.DepGraphGenerator()
    deps.Initialize(argv)

    try:
      deps_tree, deps_info = deps.GenDependencyTree()
    except SystemExit:
      oper.Error("Run of parallel_emerge exited with error while assembling"
                 " package dependencies (error message should be above).\n"
                 "Command effectively was:\n%s" %
                 ' '.join(['parallel_emerge'] + argv))
      oper.Error("Address the source of the error, then run again.")
      raise
    self._SetPortTree(deps.emerge.settings, deps.emerge.trees)
    self._deps_graph = deps.GenDependencyGraph(deps_tree, deps_info)

    cpvlist = Upgrader._GetPreOrderDepGraph(self._deps_graph)
    cpvlist.reverse()

    infolist = []
    for cpv in cpvlist:
      # See if this cpv was in target_infolist
      is_target = False
      for info in target_infolist:
        if cpv == info['cpv']:
          infolist.append(info)
          is_target = True
          break
      if not is_target:
        infolist.append(self._CreateInfoFromCPV(cpv, cpv_key='cpv'))

    return infolist

  def _FinalizeLocalInfolist(self, orig_infolist):
    """Filters and fleshes out |orig_infolist|, returns new list.

    Each info object is assumed to have entries for:
    'cpv', 'package', 'package_name', 'category'
    """
    infolist = []
    for info in orig_infolist:
      # No need to report or try to upgrade chromeos-base packages.
      if info['category'] == 'chromeos-base': continue

      dbapi = self._GetPortageDBAPI()
      ebuild_path = dbapi.findname2(info['cpv'])[0]
      (overlay, cat, pn, pv) = self._SplitEBuildPath(ebuild_path)
      ver_rev = pv.replace(pn + '-', '')
      slot, = dbapi.aux_get(info['cpv'], ['SLOT'])

      info['slot'] = slot
      info['overlay'] = overlay
      info['version_rev'] = ver_rev
      info['package_ver'] = pv

      infolist.append(info)

    return infolist

  def _FinalizeUpstreamInfolist(self, infolist):
    """Adds missing values in each upstream info in |infolist|, returns list."""

    for info in infolist:
      info['slot'] = NOT_APPLICABLE
      info['overlay'] = NOT_APPLICABLE
      info['version_rev'] = NOT_APPLICABLE
      info['package_ver'] = NOT_APPLICABLE

    return infolist

  def _ResolveAndVerifyArgs(self, args, upgrade_mode):
    """Resolve |args| to full pkgs, and check validity of each.

    Each argument will be resolved to a full category/packagename, if possible,
    by looking in both the local overlays and the upstream overlay.  Any
    argument that cannot be resolved will raise a RuntimeError.

    Arguments that specify a specific version of a package are only
    allowed when |upgrade_mode| is True.

    The 'world' target is handled as a local package.

    Any errors will raise a RuntimeError.

    Return list of package infos, one for each argument.  Each will have:
    'user_arg' = Original command line argument package was resolved from
    'package'  = Resolved category/package_name
    'package_name' = package_name
    'category' = category (None for 'world' target)
    Packages found in local overlays will also have:
    'cpv'      = Current cpv ('world' for 'world' target)
    Packages found upstream will also have:
    'upstream_cpv' = Upstream cpv
    """
    infolist = []

    for arg in args:
      info = {'user_arg': arg}

      if arg == WORLD_TARGET:
        # The 'world' target is a special case.  Consider it a valid target
        # locally, but not an upstream package.
        info['package'] = arg
        info['package_name'] = arg
        info['category'] = None
        info['cpv'] = arg
      else:
        catpkg = Upgrader._GetCatPkgFromCpv(arg)
        verrev = Upgrader._GetVerRevFromCpv(arg)

        if verrev and not upgrade_mode:
          raise RuntimeError("Specifying specific versions is only allowed "
                             "in upgrade mode.  Don't know what to do with "
                             "'%s'." % arg)

        # Local cpv search ignores version in argument, if any.  If version is
        # in argument, though, it *must* be found upstream.
        local_arg = catpkg if catpkg else arg

        local_cpv = self._FindCurrentCPV(local_arg)
        upstream_cpv = self._FindUpstreamCPV(arg, self._unstable_ok)

        # Old-style virtual packages will resolve to their target packages,
        # which we do not want here because if the package 'virtual/foo' was
        # specified at the command line we want to try upgrading the actual
        # 'virtual/foo' package, not whatever package equery resolves it to.
        # This only matters when 'virtual/foo' is currently an old-style
        # virtual but a new-style virtual for it exists upstream which we
        # want to upgrade to.  For new-style virtuals, equery will resolve
        # 'virtual/foo' to 'virtual/foo', which is fine.
        if arg.startswith('virtual/'):
          if local_cpv and not local_cpv.startswith('virtual/'):
            local_cpv = None

        if not upstream_cpv and verrev:
          # See if --unstable-ok is required for this upstream version.
          if not self._unstable_ok and self._FindUpstreamCPV(arg, True):
            raise RuntimeError("Upstream '%s' is unstable on %s.  Re-run with "
                               "--unstable-ok option?" % (arg, self._curr_arch))
          else:
            raise RuntimeError("Unable to find '%s' upstream on %s." %
                               (arg, self._curr_arch))

        any_cpv = local_cpv if local_cpv else upstream_cpv
        if any_cpv:
          self._FillInfoFromCPV(info, any_cpv)

        if local_cpv and upstream_cpv:
          oper.Info("Resolved '%s' to '%s' (local) and '%s' (upstream)." %
                    (arg, local_cpv, upstream_cpv))
          info['cpv'] = local_cpv
          info['upstream_cpv'] = upstream_cpv
        elif local_cpv:
          oper.Info("Resolved '%s' to '%s' (local)." %
                    (arg, local_cpv))
          info['cpv'] = local_cpv
        elif upstream_cpv:
          oper.Info("Resolved '%s' to '%s' (upstream)." %
                    (arg, upstream_cpv))
          info['upstream_cpv'] = upstream_cpv
        else:
          msg = ("Unable to resolve '%s' as a package either local or upstream."
                 % arg)
          if arg.find('/') < 0:
            msg = msg + " Try specifying the full category/package_name."

          raise RuntimeError(msg)

      infolist.append(info)

    return infolist

  def PrepareToRun(self):
    """Checkout upstream gentoo if necessary, and any other prep steps."""
    if not self._upstream:
      if os.path.exists(self._upstream_repo):
        # Previously created upstream cache can be re-used.  Just update it.
        oper.Info('Updating previously created upstream cache at %s.' %
                  self._upstream_repo)
        self._RunGit(self._upstream_repo, 'remote update')
      else:
        # Create local copy of upstream gentoo.
        oper.Info('Cloning origin/gentoo at %s as upstream reference.' %
                  self._upstream_repo)
        root = os.path.dirname(self._upstream_repo)
        name = os.path.basename(self._upstream_repo)
        self._RunGit(root, 'clone %s %s' % (self.PORTAGE_GIT_URL, name))

        # Create a README file to explain its presence.
        fh = open(self._upstream_repo + '-README', 'w')
        fh.write('Directory at %s is local copy of upstream '
                 'Gentoo/Portage packages. Used by cros_portage_upgrade.\n'
                 'Feel free to delete if you want the space back.\n' %
                 self._upstream_repo)
        fh.close()

      self._RunGit(self._upstream_repo, 'checkout %s' % self.ORIGIN_GENTOO,
                   redirect_stdout=True, combine_stdout_stderr=True)

    # An empty directory is needed to trick equery later.
    self._emptydir = tempfile.mkdtemp()

  def RunCompleted(self):
    """Undo any checkout of upstream gentoo if requested."""
    if not self._upstream:
      if self._no_upstream_cache:
        oper.Info('Removing upstream cache at %s as requested.'
                  % self._upstream_repo)
        shutil.rmtree(self._upstream_repo)

        # Remove the README file, too.
        readmepath = self._upstream_repo + '-README'
        if os.path.exists(readmepath):
          os.remove(readmepath)
      else:
        oper.Info('Keeping upstream cache at %s.' % self._upstream_repo)

    os.rmdir(self._emptydir)

  def CommitIsStaged(self):
    """Return True if upgrades are staged and ready for a commit."""
    return bool(self._upgrade_cnt)

  def Commit(self):
    """Commit whatever has been prepared in the stable repo."""
    # Trying to create commit message body lines that look like these:
    # Upgraded foo/bar-1.2.3 to version 1.2.4
    # Upgraded foo/bar-1.2.3 to version 1.2.4 (arm) AND version 1.2.4-r1 (x86)

    commit_lines = [] # Lines for the body of the commit message
    key_lines = []    # Lines needed in package.keywords
    pkg_overlays = {} # Overlays for upgraded packages in non-portage overlays.

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
      upgraded_versset = set()
      upgraded_verslist = []
      for arch in self._master_archs:
        upgraded_ver = row[upgraded_cols[arch]]
        if upgraded_ver:
          # Remove ~ prefix if found, and remember as unstable.
          upgraded_stable = True
          if upgraded_ver[0] == '~':
            upgraded_stable = False
            upgraded_ver = upgraded_ver[1:]

          # This package has been upgraded for this arch.
          upgraded_versset.add(upgraded_ver)
          upgraded_verslist.append(upgraded_ver)

          # Is this upgraded version unstable for this arch?  If so, save
          # arch under upgraded_cpv as one that will need a package.keywords
          # entry.
          if not upgraded_stable:
            upgraded_cpv = pkg + '-' + upgraded_ver
            cpv_key = pkg_keys.get(upgraded_cpv, set())
            cpv_key.add(arch)
            pkg_keys[upgraded_cpv] = cpv_key

          # Save the overlay this package is originally from, if the overlay
          # is not a Portage overlay (e.g. chromiumos-overlay).
          ovrly_col = utable.UpgradeTable.COL_OVERLAY
          ovrly_col = utable.UpgradeTable.GetColumnName(ovrly_col, arch)
          ovrly = row[ovrly_col]
          if (ovrly != NOT_APPLICABLE and
              ovrly != self.UPSTREAM_OVERLAY_NAME and
              ovrly != self.STABLE_OVERLAY_NAME):
            pkg_overlays[pkg] = ovrly

      if len(upgraded_versset) == 1:
        # Upgrade is the same across all archs.
        upgraded_ver = upgraded_versset.pop()
        arch_str = ', '.join(sorted(self._master_archs))
        pkg_commit_line = ('%s %s to version %s on %s' %
                           (UPGRADED, pkg, upgraded_ver, arch_str))

      elif len(upgraded_versset) > 1:
        # Iterate again, and specify arch for each upgraded version.
        tokens = []
        for upgraded_ver in upgraded_verslist:
          tokens.append('%s on %s' % (upgraded_ver, arch))
        pkg_commit_line = ('%s %s to versions %s' %
                           (UPGRADED, pkg, ' AND '.join(tokens)))

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

      oper.Warning('\n'
                   'Upgrade changes committed (see above),'
                   ' but message needs edit BY YOU:\n'
                   ' cd %s; git commit --amend; cd -' %
                   self._stable_repo)
      # See if any upgraded packages are in non-portage overlays now, meaning
      # they probably require a patch and should not go into portage-stable.
      if pkg_overlays:
        lines = ['%s [%s]' % (p, pkg_overlays[p]) for p in pkg_overlays]
        oper.Warning('\n'
                     'The following packages were coming from a non-portage'
                     ' overlay, which means they were probably patched.\n'
                     'You should consider whether the upgraded package'
                     ' needs the same patches applied now.\n'
                     'If so, do not commit these changes in portage-stable.'
                     ' Instead, copy them to the applicable overlay dir.\n'
                     '%s' %
                     '\n'.join(lines))
      oper.Info('\n'
                'To remove any individual file above from commit do:\n'
                ' cd %s; git reset HEAD~ <filepath>; rm <filepath>;'
                ' git commit --amend -C HEAD; cd -' %
                self._stable_repo)

      if key_lines:
        oper.Warning('\n'
                     'Because one or more unstable versions are involved'
                     ' you must add line(s) like the following to\n'
                     ' %s:\n%s\n'
                     'This is needed before testing, and should be pushed'
                     ' in a separate changelist BEFORE the\n'
                     'the changes in portage-stable.' %
                     (self._GetPkgKeywordsFile(),
                      '\n'.join(key_lines)))

      oper.Info('\n'
                'If you wish to undo all the changes to %s:\n'
                ' cd %s; git reset --hard HEAD~; cd -' %
                (self.STABLE_OVERLAY_NAME, self._stable_repo))

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
    upgrade_mode = self._IsInUpgradeMode()
    self._curr_table = utable.UpgradeTable(self._curr_arch,
                                           upgrade=upgrade_mode,
                                           name=board)

    if self._AnyChangesStaged():
      self._StashChanges()

    try:
      target_infolist = self._ResolveAndVerifyArgs(self._args, upgrade_mode)
      upstream_only_infolist = [i for i in target_infolist if not i['cpv']]
      if not upgrade_mode and upstream_only_infolist:
        # This means that not all arguments were found in local source, which is
        # only allowed in upgrade mode.
        msg = ("The following packages were not found in current overlays"
               " (but they do exist upstream):\n%s" %
               '\n'.join([info['user_arg'] for info in upstream_only_infolist]))
        raise RuntimeError(msg)

      full_infolist = None

      if self._upgrade:
        # Shallow upgrade mode only cares about targets as they were
        # found upstream.
        full_infolist = self._FinalizeUpstreamInfolist(target_infolist)
      else:
        # Assembling dependencies only matters in status report mode or
        # if --upgrade-deep was requested.
        local_target_infolist = [i for i in target_infolist if i['cpv']]
        if local_target_infolist:
          oper.Info("Assembling package dependencies.")
          full_infolist = self._GetCurrentVersions(local_target_infolist)
          full_infolist = self._FinalizeLocalInfolist(full_infolist)
        else:
          full_infolist = []

        # Append any command line targets that were not found in current
        # overlays. The idea is that they will still be found upstream
        # for upgrading.
        if upgrade_mode:
          tmp_list = self._FinalizeUpstreamInfolist(upstream_only_infolist)
          full_infolist = full_infolist + tmp_list

      self._UnstashAnyChanges()
      self._UpgradePackages(full_infolist)

    finally:
      self._DropAnyStashedChanges()

    # Merge tables together after each run.
    self._master_cnt += 1
    self._master_archs.add(self._curr_arch)
    if self._master_table:
      tables = [self._master_table, self._curr_table]
      self._master_table = mps.MergeTables(tables)
    else:
      self._master_table = self._curr_table
      self._master_table._arch = None

  def WriteTableFiles(self, csv=None):
    """Write |self._master_table| to |csv| file, if requested."""

    # Sort the table by package name, then slot
    def PkgSlotSort(row):
      return (row[self._master_table.COL_PACKAGE],
              row[self._master_table.COL_SLOT])
    self._master_table.Sort(PkgSlotSort)

    if csv:
      filehandle = open(csv, 'w')
      oper.Info('Writing package status as csv to %s.' % csv)
      self._master_table.WriteCSV(filehandle)
      filehandle.close()
    elif not self._IsInUpgradeMode():
      oper.Info('Package status report file not requested (--to-csv).')

  def SayGoodbye(self):
    """Print any final messages to user."""
    if not self._IsInUpgradeMode():
      # Without this message users are confused why running a script
      # with 'upgrade' in the name does not actually do an upgrade.
      oper.Warning("Completed status report run.  To run in 'upgrade'"
                   " mode include the --upgrade option.")

def _BoardIsSetUp(board):
  """Return true if |board| has been setup."""
  return os.path.isdir('/build/%s' % board)

def _CreateOptParser():
  """Create the optparser.parser object for command-line args."""
  usage = 'Usage: %prog [options] packages...'
  epilog = ('\n'
            'There are essentially two "modes": status report mode and '
            'upgrade mode.\nStatus report mode is the default; upgrade '
            'mode is enabled by either --upgrade or --upgrade-deep.\n'
            '\n'
            'In either mode, packages can be specified in any manner '
            'commonly accepted by Portage tools.  For example:\n'
            ' category/package_name\n'
            ' package_name\n'
            ' category/package_name-version (upgrade mode only)\n'
            '\n'
            'Status report mode will report on the status of the specified '
            'packages relative to upstream,\nwithout making any changes. '
            'In this mode, the specified packages are often high-level\n'
            'targets such as "chromeos" or "chromeos-dev". '
            'The --to-csv option is often used in this mode.\n'
            'The --unstable-ok option in this mode will make '
            'the upstream comparison (e.g. "needs update") be\n'
            'relative to the latest upstream version, stable or not.\n'
            '\n'
            'Upgrade mode will attempt to upgrade the specified '
            'packages to one of the following versions:\n'
            '1) The version specified in argument (e.g. foo/bar-1.2.3)\n'
            '2) The latest stable version upstream (the default)\n'
            '3) The latest overall version upstream (with --unstable-ok)\n'
            '\n'
            'Unlike with --upgrade, if --upgrade-deep is specified, '
            'then the package dependencies will also be upgraded.\n'
            'In upgrade mode, it is ok if the specified packages only '
            'exist upstream.\n'
            'The --force option can be used to do a package upgrade '
            'even if the local version matches the upstream version.\n'
            '\n'
            'Status report mode examples:\n'
            '> cros_portage_upgrade --board=arm-generic:x86-generic '
            '--to-csv=cros-aebl.csv chromeos\n'
            '> cros_portage_upgrade --unstable-ok --board=x86-mario '
            '--to-csv=cros_test-mario chromeos chromeos-dev chromeos-test\n'
            'Upgrade mode examples:\n'
            '> cros_portage_upgrade --board=arm-generic:x86-generic '
            '--upgrade sys-devel/gdb virtual/yacc\n'
            '> cros_portage_upgrade --unstable-ok --board=x86-mario '
            '--upgrade-deep gdata\n'
            '> cros_portage_upgrade --board=x86-generic --upgrade '
            'media-libs/libpng-1.2.45\n'
            '\n'
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
                    default=None, help="Target board(s), colon-separated")
  parser.add_option('--force', dest='force', action='store_true',
                    default=False,
                    help="Force upgrade even if version already in source")
  parser.add_option('--host', dest='host', action='store_true',
                    default=False,
                    help="Host target pseudo-board")
  parser.add_option('--no-upstream-cache', dest='no_upstream_cache',
                    action='store_true', default=False,
                    help="Do not preserve cached upstream for future runs")
  parser.add_option('--rdeps', dest='rdeps', action='store_true',
                    default=False,
                    help="Use runtime dependencies only")
  parser.add_option('--srcroot', dest='srcroot', type='string', action='store',
                    default='%s/trunk/src' % os.environ['HOME'],
                    help="Path to root src directory [default: '%default']")
  parser.add_option('--to-csv', dest='csv_file', type='string', action='store',
                    default=None, help="File to write csv-formatted results to")
  parser.add_option('--upgrade', dest='upgrade',
                    action='store_true', default=False,
                    help="Upgrade target package(s) only")
  parser.add_option('--upgrade-deep', dest='upgrade_deep', action='store_true',
                    default=False,
                    help="Upgrade target package(s) and all dependencies")
  # TODO(mtennant): Put UPSTREAM_TMP_REPO in as default for better transparency.
  parser.add_option('--upstream', dest='upstream', type='string',
                    action='store', default=None,
                    help="Latest upstream repo location [default: '%default']")
  parser.add_option('--unstable-ok', dest='unstable_ok', action='store_true',
                    default=False,
                    help="Use latest upstream ebuild, stable or not")
  parser.add_option('--verbose', dest='verbose', action='store_true',
                    default=False,
                    help="Enable verbose output (for debugging)")

  return parser


def main():
  """Main function."""
  parser = _CreateOptParser()
  (options, args) = parser.parse_args()

  if (options.verbose): logging.basicConfig(level=logging.DEBUG)

  #
  # Do some argument checking.
  #

  if not options.board and not options.host:
    parser.print_help()
    oper.Die('Board (or host) is required.')

  if not args:
    parser.print_help()
    oper.Die('No packages provided.')

  # The --upgrade and --upgrade-deep options are mutually exclusive.
  if options.upgrade_deep and options.upgrade:
    parser.print_help()
    oper.Die('The --upgrade and --upgrade-deep options ' +
             'are mutually exclusive.')

  # The --force option only makes sense with --upgrade or --upgrade-deep.
  if options.force and not (options.upgrade or options.upgrade_deep):
    parser.print_help()
    oper.Die('The --force option requires --upgrade or --upgrade-deep.')

  # The --upstream and --no-upstream-cache options are mutually exclusive.
  if options.upstream and options.no_upstream_cache:
    parser.print_help()
    oper.Die('The --upstream and --no-upstream-cache options ' +
             'are mutually exclusive.')

  # If upstream portage is provided, verify that it is a valid directory.
  if options.upstream and not os.path.isdir(options.upstream):
    parser.print_help()
    oper.Die('Argument to --upstream must be a valid directory.')

  # If --to-csv given verify file can be opened for write.
  if options.csv_file:
    try:
      fh = open(options.csv_file, 'w')
      fh.close()
    except IOError as ex:
      parser.print_help()
      oper.Die('Unable to open %s for writing: %s' % (options.csv_file,
                                                      str(ex)))

  upgrader = Upgrader(options, args)
  upgrader.PreRunChecks()

  # Automatically handle board 'host' as 'amd64-host'.
  boards = []
  if options.board:
    boards = options.board.split(':')
    boards = [Upgrader.HOST_BOARD if b == 'host' else b for b in boards]

  # Make sure host pseudo-board is run first.
  if options.host and Upgrader.HOST_BOARD not in boards:
    boards.insert(0, Upgrader.HOST_BOARD)
  elif Upgrader.HOST_BOARD in boards:
    boards = [b for b in boards if b != Upgrader.HOST_BOARD]
    boards.insert(0, Upgrader.HOST_BOARD)

  # Check that all boards have been setup first.
  for board in boards:
    if (board != Upgrader.HOST_BOARD and not _BoardIsSetUp(board)):
      parser.print_help()
      oper.Die('You must setup the %s board first.' % board)

  passed = True
  try:
    upgrader.PrepareToRun()

    for board in boards:
      oper.Info('Running with board %s.' % board)
      upgrader.RunBoard(board)
  except RuntimeError as ex:
    passed = False
    oper.Error(str(ex))

  finally:
    upgrader.RunCompleted()

  if not passed:
    oper.Die("Failed with above errors.")

  if upgrader.CommitIsStaged():
    upgrader.Commit()

  # TODO(mtennant): Move stdout output to here, rather than as-we-go.  That
  # way it won't come out for each board.  Base it on contents of final table.
  # Make verbose-dependent?

  upgrader.WriteTableFiles(csv=options.csv_file)

  upgrader.SayGoodbye()

if __name__ == '__main__':
  main()
