#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs Chrome for cbuildbot.

After calling, it prints outs CHROME_VERSION_ATOM=(version atom string).  A
caller could then use this atom with emerge to build the newly uprevved version
of Chrome e.g.

./cros_mark_chrome_as_stable tot
Returns chrome-base/chromeos-chrome-8.0.552.0_alpha_r1

emerge-x86-generic =chrome-base/chromeos-chrome-8.0.552.0_alpha_r1
"""

import constants
import filecmp
import optparse
import os
import re
import sys
import time

import cros_mark_as_stable
from cros_build_lib import RunCommand, Info, Warning

BASE_CHROME_SVN_URL = 'http://src.chromium.org/svn'

# Helper regex's for finding ebuilds.
_CHROME_VERSION_REGEX = '\d+\.\d+\.\d+\.\d+'
_NON_STICKY_REGEX = '%s[(_rc.*)|(_alpha.*)]+' % _CHROME_VERSION_REGEX

# Dir where all the action happens.
_CHROME_OVERLAY_DIR = ('%(srcroot)s/third_party/chromiumos-overlay'
                       '/chromeos-base/chromeos-chrome')

_GIT_COMMIT_MESSAGE = ('Marking %(chrome_rev)s for chrome ebuild with version '
                       '%(chrome_version)s as stable.')

# URLs that print lists of chrome revisions between two versions of the browser.
_CHROME_VERSION_URL = ('http://omahaproxy.appspot.com/changelog?'
                       'old_version=%(old)s&new_version=%(new)s')
_CHROME_REVISION_URL = ('http://build.chromium.org/f/chromium/perf/dashboard/'
                        'ui/changelog.html?url=&range=%(old)s%%3A%(new)s'
                        '&mode=html')

_CHROME_SVN_TAG = 'CROS_SVN_COMMIT'

def _GetSvnUrl(base_url):
  """Returns the path to the svn url for the given chrome branch."""
  return os.path.join(base_url, 'trunk')


def  _GetTipOfTrunkSvnRevision(base_url):
  """Returns the current svn revision for the chrome tree."""
  svn_url = _GetSvnUrl(base_url)
  svn_info = RunCommand(['svn', 'info', svn_url], redirect_stdout=True)

  revision_re = re.compile('^Revision:\s+(\d+).*')
  for line in svn_info.splitlines():
    match = revision_re.search(line)
    if match:
      svn_revision = match.group(1)
      Info('Found SVN Revision %s' % svn_revision)
      return svn_revision

  raise Exception('Could not find revision information from %s' % svn_url)


def _GetVersionContents(chrome_version_info):
  """Returns the current Chromium version, from the contents of a VERSION file.

  Args:
     chrome_version_info: The contents of a chromium VERSION file.
  """
  chrome_version_array = []
  for line in chrome_version_info.splitlines():
    chrome_version_array.append(line.rpartition('=')[2])

  return '.'.join(chrome_version_array)

def _GetSpecificVersionUrl(base_url, revision, time_to_wait=600):
  """Returns the Chromium version, from a repository URL and version.

  Args:
     base_url: URL for the root of the chromium checkout.
     revision: the SVN revision we want to use.
     time_to_wait: the minimum period before abandoning our wait for the
         desired revision to be present.
  """
  svn_url = os.path.join(_GetSvnUrl(base_url), 'src', 'chrome', 'VERSION')
  if not revision or not (int(revision) > 0):
    raise Exception('Revision must be positive, got %s' % revision)

  start = time.time()
  # Use the fact we are SVN, hence ordered.
  # Dodge the fact it will silently ignore the revision if it is not
  # yet known.  (i.e. too high)
  repo_version = _GetTipOfTrunkSvnRevision(base_url)
  while revision > repo_version:
    if time.time() - start > time_to_wait:
      raise Exception('Timeout Exceeeded')

    Info('Repository only has version %s, looking for %s.  Sleeping...' %
         (repo_version, revision))
    time.sleep(30)
    repo_version = _GetTipOfTrunkSvnRevision(base_url)

  chrome_version_info = RunCommand(
      ['svn', 'cat', '-r', revision, svn_url],
      redirect_stdout=True,
      error_message='Could not read version file at %s revision %s.' %
                    (svn_url, revision))

  return _GetVersionContents(chrome_version_info)


def _GetTipOfTrunkVersionFile(root):
  """Returns the current Chromium version, from a file in a checkout.

  Args:
     root: path to the root of the chromium checkout.
  """
  version_file = os.path.join(root, 'src', 'chrome', 'VERSION')
  chrome_version_info = RunCommand(
      ['cat', version_file],
      redirect_stdout=True,
      error_message='Could not read version file at %s.' % version_file)

  return _GetVersionContents(chrome_version_info)

def _GetLatestRelease(base_url, branch=None):
  """Gets the latest release version from the buildspec_url for the branch.

  Args:
    branch:  If set, gets the latest release for branch, otherwise latest
      release.
  Returns:
    Latest version string.
  """
  buildspec_url = os.path.join(base_url, 'releases')
  svn_ls = RunCommand(['svn', 'ls', buildspec_url], redirect_stdout=True)
  sorted_ls = RunCommand(['sort', '--version-sort', '-r'], input=svn_ls,
                         redirect_stdout=True)
  if branch:
    chrome_version_re = re.compile('^%s\.\d+.*' % branch)
  else:
    chrome_version_re = re.compile('^[0-9]+\..*')

  for chrome_version in sorted_ls.splitlines():
    if chrome_version_re.match(chrome_version) and RunCommand(['svn', 'ls',
        os.path.join(buildspec_url, chrome_version, 'DEPS')],
        error_ok=True, redirect_stdout=True) == 'DEPS\n':
      return chrome_version.rstrip('/')

  return None


def _GetStickyEBuild(stable_ebuilds):
  """Returns the sticky ebuild."""
  sticky_ebuilds = []
  non_sticky_re = re.compile(_NON_STICKY_REGEX)
  for ebuild in stable_ebuilds:
    if not non_sticky_re.match(ebuild.version):
      sticky_ebuilds.append(ebuild)

  if not sticky_ebuilds:
    raise Exception('No sticky ebuilds found')
  elif len(sticky_ebuilds) > 1:
    Warning('More than one sticky ebuild found')

  return cros_mark_as_stable.BestEBuild(sticky_ebuilds)


class ChromeEBuild(cros_mark_as_stable.EBuild):
  """Thin sub-class of EBuild that adds a chrome_version field."""
  chrome_version_re = re.compile('.*chromeos-chrome-(%s|9999).*' % (
      _CHROME_VERSION_REGEX))
  chrome_version = ''

  def __init__(self, path):
    cros_mark_as_stable.EBuild.__init__(self, path)
    re_match = self.chrome_version_re.match(self.ebuild_path_no_revision)
    if re_match:
      self.chrome_version = re_match.group(1)

  def __cmp__(self, other):
    """Use ebuild paths for comparison."""
    if self.ebuild_path == other.ebuild_path:
      return 0
    elif self.ebuild_path > other.ebuild_path:
      return 1
    else:
      return (-1)

  def __str__(self):
    return self.ebuild_path


def FindChromeCandidates(overlay_dir):
  """Return a tuple of chrome's unstable ebuild and stable ebuilds.

  Args:
    overlay_dir: The path to chrome's portage overlay dir.
  Returns:
    Tuple [unstable_ebuild, stable_ebuilds].
  Raises:
    Exception: if no unstable ebuild exists for Chrome.
  """
  stable_ebuilds = []
  unstable_ebuilds = []
  for path in [
      os.path.join(overlay_dir, entry) for entry in os.listdir(overlay_dir)]:
    if path.endswith('.ebuild'):
      ebuild = ChromeEBuild(path)
      if not ebuild.chrome_version:
        Warning('Poorly formatted ebuild found at %s' % path)
      else:
        if '9999' in ebuild.version:
          unstable_ebuilds.append(ebuild)
        else:
          stable_ebuilds.append(ebuild)

  # Apply some sanity checks.
  if not unstable_ebuilds:
    raise Exception('Missing 9999 ebuild for %s' % overlay_dir)
  if not stable_ebuilds:
    Warning('Missing stable ebuild for %s' % overlay_dir)

  return cros_mark_as_stable.BestEBuild(unstable_ebuilds), stable_ebuilds


def FindChromeUprevCandidate(stable_ebuilds, chrome_rev, sticky_branch):
  """Finds the Chrome uprev candidate for the given chrome_rev.

  Using the pre-flight logic, this means the stable ebuild you are uprevving
  from.  The difference here is that the version could be different and in
  that case we want to find it to delete it.

  Args:
    stable_ebuilds: A list of stable ebuilds.
    chrome_rev: The chrome_rev designating which candidate to find.
    sticky_branch:  The the branch that is currently sticky with Major/Minor
      components.  For example: 9.0.553
  Returns:
    Returns the EBuild, otherwise None if none found.
  """
  candidates = []
  if chrome_rev in [constants.CHROME_REV_LOCAL, constants.CHROME_REV_TOT,
                    constants.CHROME_REV_SPEC]:
    # These are labelled alpha, for historic reasons,
    # not just for the fun of confusion.
    chrome_branch_re = re.compile('%s.*_alpha.*' % _CHROME_VERSION_REGEX)
    for ebuild in stable_ebuilds:
      if chrome_branch_re.search(ebuild.version):
        candidates.append(ebuild)

  elif chrome_rev == constants.CHROME_REV_STICKY:
    chrome_branch_re = re.compile('%s\..*' % sticky_branch)
    for ebuild in stable_ebuilds:
      if chrome_branch_re.search(ebuild.version):
        candidates.append(ebuild)

  else:
    chrome_branch_re = re.compile('%s.*_rc.*' % _CHROME_VERSION_REGEX)
    for ebuild in stable_ebuilds:
      if chrome_branch_re.search(ebuild.version) and (
          not ebuild.chrome_version.startswith(sticky_branch)):
        candidates.append(ebuild)

  if candidates:
    return cros_mark_as_stable.BestEBuild(candidates)
  else:
    return None

def _AnnotateAndPrint(text, url):
  """Add buildbot trappings to print <a href='url'>text</a> in the waterfall.

  Args:
    text: Anchor text for the link
    url: the URL to which to link
  """
  print >> sys.stderr, '\n@@@STEP_LINK@%(text)s@%(url)s@@@' % { 'text': text,
                                                              'url': url }

def GetChromeRevisionLinkFromVersions(old_chrome_version, chrome_version):
  """Return appropriately formatted link to revision info, given versions

  Given two chrome version strings (e.g. 9.0.533.0), generate a link to a
  page that prints the Chromium revisions between those two versions.

  Args:
    old_chrome_version: version to diff from
    chrome_version: version to which to diff
  Returns:
    The desired URL.
  """
  return _CHROME_VERSION_URL % { 'old': old_chrome_version,
                                 'new': chrome_version }

def GetChromeRevisionLinkFromRevisions(old_chrome_rev, chrome_rev):
  """Return appropriately formatted link to revision info, given SVN revisions

  Given two SVN revisions (e.g. 12345), generate a link to a page that
  prints the Chromium changes between those two revisions, inclusive.

  Args:
    old_chrome_rev: rev to diff from
    chrome_rev: rev to which to diff
  Returns:
    The desired URL.
  """
  return _CHROME_REVISION_URL % { 'old': old_chrome_rev, 'new': chrome_rev }

def GetRevisionFromEBuild(chrome_ebuild):
  """Given a ChromeEBuild, find the SVN revision it builds Chrome at, if any.

  Chromium ebuilds for ToT chrome contain an SVN revision, at which the code
  is checked out and built.  Given a ChromeEBuild, find the file that backs it
  and parse out said revision number.
  Args:
    chrome_ebuild: A ChromeEBuild wrapping a chrome ebuild to parse.
  Returns:
    The SVN revision found, if any.  Else, None
  """
  try:
    ebuild = open(chrome_ebuild.ebuild_path)
  except (OSError, IOError) as e:
    Warning('%s cannot be read due to %s' % (chrome_ebuild.ebuild_path, e))
    raise
  chrome_revision_re = re.compile('^%s="?(\d+)"?$' % _CHROME_SVN_TAG)
  for line in ebuild:
    re_match = chrome_revision_re.match(line)
    if re_match:
      return re_match.group(1)
  ebuild.close()
  return None

def GetChromeRevisionListLink(old_chrome, new_chrome, chrome_rev):
  """Returns a link to the list of revisions between two Chromium versions

  Given two ChromeEBuilds and the kind of rev we're doing, generate a
  link to a page that prints the Chromium changes between those two
  revisions, inclusive.

  Args:
    old_chrome: ebuild for the version to diff from
    new_chrome: ebuild for the version to which to diff
    chrome_rev: one of constants.VALID_CHROME_REVISIONS
  Returns:
    The desired URL.
  """
  if chrome_rev in [constants.CHROME_REV_TOT, constants.CHROME_REV_SPEC]:
    return GetChromeRevisionLinkFromRevisions(GetRevisionFromEBuild(old_chrome),
                                              GetRevisionFromEBuild(new_chrome))
  elif chrome_rev == constants.CHROME_REV_LOCAL:
    return 'data:text/plain;,Sorry%2C+Local+changes%2e'
  else:
    return GetChromeRevisionLinkFromVersions(old_chrome.chrome_version,
                                             new_chrome.chrome_version)

def MarkChromeEBuildAsStable(stable_candidate, unstable_ebuild, chrome_rev,
                             chrome_version, commit, overlay_dir,
                             sticky_ebuild):
  """Uprevs the chrome ebuild specified by chrome_rev.

  This is the main function that uprevs the chrome_rev from a stable candidate
  to its new version.

  Args:
    stable_candidate: ebuild that corresponds to the stable ebuild we are
      revving from.  If None, builds the a new ebuild given the version
      and logic for chrome_rev type with revision set to 1.
    unstable_ebuild:  ebuild corresponding to the unstable ebuild for chrome.
    chrome_rev: one of constants.VALID_CHROME_REVISIONS or LOCAL
      constants.CHROME_REV_SPEC -  Requires commit value.  Revs the ebuild for
        the specified version and uses the portage suffix of _alpha.
      constants.CHROME_REV_TOT -  Requires commit value.  Revs the ebuild for
        the TOT version and uses the portage suffix of _alpha.
      constants.CHROME_REV_LOCAL - Requires a chrome_root. Revs the ebuild for
        the local version and uses the portage suffix of _alpha.
      constants.CHROME_REV_LATEST - This uses the portage suffix of _rc as they
        are release candidates for the next sticky version.
      constants.CHROME_REV_STICKY -  Revs the sticky version.
    chrome_version:  The \d.\d.\d.\d version of Chrome.
    commit:  Used with constants.CHROME_REV_TOT.  The svn revision of chrome.
    overlay_dir:  Path to the chromeos-chrome package dir.
    sticky_ebuild: EBuild class for the sticky ebuild.
  Returns:
    Full portage version atom (including rc's, etc) that was revved.
  """
  def IsTheNewEBuildRedundant(new_ebuild, stable_ebuild):
    """Returns True if the new ebuild is redundant.

    This is True if there if the current stable ebuild is the exact same copy
    of the new one OR the chrome versions are the same and we're revving
    constants.CHROME_REV_LATEST (as we don't care about 9999 changes for it).
    """
    if not stable_ebuild:
      return False

    if stable_candidate.chrome_version == new_ebuild.chrome_version:
      if chrome_rev == constants.CHROME_REV_LATEST:
        return True
      else:
        return filecmp.cmp(
            new_ebuild.ebuild_path, stable_ebuild.ebuild_path, shallow=False)

  base_path = os.path.join(overlay_dir, 'chromeos-chrome-%s' % chrome_version)
  # Case where we have the last stable candidate with same version just rev.
  if stable_candidate and stable_candidate.chrome_version == chrome_version:
    new_ebuild_path = '%s-r%d.ebuild' % (
        stable_candidate.ebuild_path_no_revision,
        stable_candidate.current_revision + 1)
  else:
    if chrome_rev in [constants.CHROME_REV_LOCAL, constants.CHROME_REV_TOT,
                      constants.CHROME_REV_SPEC]:
      portage_suffix = '_alpha'
    else:
      portage_suffix = '_rc'

    new_ebuild_path = base_path + ('%s-r1.ebuild' % portage_suffix)

  # Mark latest release and sticky branches as stable.
  mark_stable = chrome_rev not in [constants.CHROME_REV_TOT,
                                   constants.CHROME_REV_SPEC,
                                   constants.CHROME_REV_LOCAL]

  cros_mark_as_stable.EBuildStableMarker.MarkAsStable(
      unstable_ebuild.ebuild_path, new_ebuild_path, _CHROME_SVN_TAG, commit,
      make_stable=mark_stable)
  new_ebuild = ChromeEBuild(new_ebuild_path)

  # Determine whether this is ebuild is redundant.
  if IsTheNewEBuildRedundant(new_ebuild, stable_candidate):
    Info('Previous ebuild with same version found and ebuild is redundant.')
    os.unlink(new_ebuild_path)
    return None

  if stable_candidate and chrome_rev != constants.CHROME_REV_LOCAL:
    _AnnotateAndPrint('Chromium revisions',
                      GetChromeRevisionListLink(stable_candidate,
                                                new_ebuild,
                                                chrome_rev))

  RunCommand(['git', 'add', new_ebuild_path])
  if stable_candidate and stable_candidate != sticky_ebuild:
    RunCommand(['git', 'rm', stable_candidate.ebuild_path])

  cros_mark_as_stable.EBuildStableMarker.CommitChange(
      _GIT_COMMIT_MESSAGE % {'chrome_rev': chrome_rev,
                             'chrome_version': chrome_version})

  return '%s-%s' % (new_ebuild.package, new_ebuild.version)


def ParseMaxRevision(revision_list):
  """Returns the max revision from a list of url@revision string."""
  revision_re = re.compile('.*@(\d+)')

  def RevisionKey(revision):
    return revision_re.match(revision).group(1)

  max_revision = max(revision_list.split(), key=RevisionKey)
  return max_revision.rpartition('@')[2]


def main():
  usage_options = '|'.join(constants.VALID_CHROME_REVISIONS)
  usage = '%s OPTIONS [%s]' % (__file__, usage_options)
  parser = optparse.OptionParser(usage)
  parser.add_option('-b', '--board', default='x86-generic')
  parser.add_option('-c', '--chrome_url', default=BASE_CHROME_SVN_URL)
  parser.add_option('-f', '--force_revision', default=None)
  parser.add_option('-s', '--srcroot', default=os.path.join(os.environ['HOME'],
                                                            'trunk', 'src'),
                    help='Path to the src directory')
  parser.add_option('-t', '--tracking_branch', default='cros/master',
                    help='Branch we are tracking changes against')
  (options, args) = parser.parse_args()

  if len(args) != 1 or args[0] not in constants.VALID_CHROME_REVISIONS:
    parser.error('Commit requires arg set to one of %s.'
                 % constants.VALID_CHROME_REVISIONS)

  overlay_dir = os.path.abspath(_CHROME_OVERLAY_DIR %
                                {'srcroot': options.srcroot})
  chrome_rev = args[0]
  version_to_uprev = None
  commit_to_use = None

  (unstable_ebuild, stable_ebuilds) = FindChromeCandidates(overlay_dir)
  sticky_ebuild = _GetStickyEBuild(stable_ebuilds)
  sticky_version = sticky_ebuild.chrome_version
  sticky_branch = sticky_version.rpartition('.')[0]

  if chrome_rev == constants.CHROME_REV_LOCAL:
    if 'CHROME_ROOT' in os.environ:
      chrome_root = os.environ['CHROME_ROOT']
    else:
      chrome_root = os.path.join(os.environ['HOME'], 'chrome_root')

    version_to_uprev = _GetTipOfTrunkVersionFile(chrome_root)
    commit_to_use = 'Unknown'
    Info('Using local source, versioning is untrustworthy.')
  elif chrome_rev == constants.CHROME_REV_SPEC:
    # TODO(sosa): Buildbot may pass url@revision, check and fix commit.
    commit_to_use = options.force_revision
    if '@' in commit_to_use: commit_to_use = ParseMaxRevision(commit_to_use)
    version_to_uprev = _GetSpecificVersionUrl(options.chrome_url,
                                              commit_to_use)
  elif chrome_rev == constants.CHROME_REV_TOT:
    commit_to_use = _GetTipOfTrunkSvnRevision(options.chrome_url)
    version_to_uprev = _GetSpecificVersionUrl(options.chrome_url,
                                              commit_to_use)
  elif chrome_rev == constants.CHROME_REV_LATEST:
    version_to_uprev = _GetLatestRelease(options.chrome_url)
    # Don't rev on stable branch for latest_release.
    if re.match('%s\.\d+' % sticky_branch, version_to_uprev):
      Info('Latest release is sticky branch.  Nothing to do.')
      return
  else:
    version_to_uprev = _GetLatestRelease(options.chrome_url, sticky_branch)

  stable_candidate = FindChromeUprevCandidate(stable_ebuilds, chrome_rev,
                                              sticky_branch)

  if stable_candidate:
    Info('Stable candidate found %s' % stable_candidate)
  else:
    Info('No stable candidate found.')

  tracking_branch = 'remotes/m/%s' % os.path.basename(options.tracking_branch)
  os.chdir(overlay_dir)
  work_branch = cros_mark_as_stable.GitBranch(constants.STABLE_EBUILD_BRANCH,
                                              tracking_branch)
  work_branch.CreateBranch()
  chrome_version_atom = MarkChromeEBuildAsStable(
      stable_candidate, unstable_ebuild, chrome_rev, version_to_uprev,
      commit_to_use, overlay_dir, sticky_ebuild)
  # Explicit print to communicate to caller.
  if chrome_version_atom:
    cros_mark_as_stable.CleanStalePackages(options.board, [chrome_version_atom])
    print 'CHROME_VERSION_ATOM=%s' % chrome_version_atom


if __name__ == '__main__':
  main()
