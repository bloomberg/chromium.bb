# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various deprecated commands a builder could run."""


def _GetAllGitRepos(buildroot, debug=False):
  """Returns a list of tuples containing [git_repo, src_path]."""
  manifest_tuples = []
  # Gets all the git repos from a full repo manifest.
  repo_cmd = "repo manifest -o -".split()
  output = OldRunCommand(repo_cmd, cwd=buildroot, redirect_stdout=True,
                      redirect_stderr=True, print_cmd=debug)

  # Extract all lines containg a project.
  extract_cmd = ['grep', 'project name=']
  output = OldRunCommand(extract_cmd, cwd=buildroot, input=output,
                      redirect_stdout=True, print_cmd=debug)
  # Parse line using re to get tuple.
  result_array = re.findall('.+name=\"([\w-]+)\".+path=\"(\S+)".+', output)

  # Create the array.
  for result in result_array:
    if len(result) != 2:
      Warning('Found incorrect xml object %s' % result)
    else:
      # Remove pre-pended src directory from manifest.
      manifest_tuples.append([result[0], result[1].replace('src/', '')])

  return manifest_tuples


def _GetCrosWorkOnSrcPath(buildroot, board, package, debug=False):
  """Returns ${CROS_WORKON_SRC_PATH} for given package."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  equery_cmd = ('equery-%s which %s' % (board, package)).split()
  ebuild_path = OldRunCommand(equery_cmd, cwd=cwd, redirect_stdout=True,
                             redirect_stderr=True, enter_chroot=True,
                             error_ok=True, print_cmd=debug)
  if ebuild_path:
    ebuild_cmd = ('ebuild-%s %s info' % (board, ebuild_path)).split()
    cros_workon_output = OldRunCommand(ebuild_cmd, cwd=cwd,
                                    redirect_stdout=True, redirect_stderr=True,
                                    enter_chroot=True, print_cmd=debug)

    temp = re.findall('CROS_WORKON_SRCDIR="(\S+)"', cros_workon_output)
    if temp:
      return temp[0]

  return None


def _CreateRepoDictionary(buildroot, board, debug=False):
  """Returns the repo->list_of_ebuilds dictionary."""
  repo_dictionary = {}
  manifest_tuples = _GetAllGitRepos(buildroot)
  Info('Creating dictionary of git repos to portage packages ...')

  cwd = os.path.join(buildroot, 'src', 'scripts')
  get_all_workon_pkgs_cmd = './cros_workon list --all'.split()
  packages = OldRunCommand(get_all_workon_pkgs_cmd, cwd=cwd,
                        redirect_stdout=True, redirect_stderr=True,
                        enter_chroot=True, print_cmd=debug)
  for package in packages.split():
    cros_workon_src_path = _GetCrosWorkOnSrcPath(buildroot, board, package)
    if cros_workon_src_path:
      for tuple in manifest_tuples:
        # This path tends to have the user's home_dir prepended to it.
        if cros_workon_src_path.endswith(tuple[1]):
          Info('For %s found matching package %s' % (tuple[0], package))
          if repo_dictionary.has_key(tuple[0]):
            repo_dictionary[tuple[0]] += [package]
          else:
            repo_dictionary[tuple[0]] = [package]

  return repo_dictionary


def _ParseRevisionString(revision_string, repo_dictionary):
  """Parses the given revision_string into a revision dictionary.

  Returns a list of tuples that contain [portage_package_name, commit_id] to
  update.

  Keyword arguments:
  revision_string -- revision_string with format
      'repo1.git@commit_1 repo2.git@commit2 ...'.
  repo_dictionary -- dictionary with git repository names as keys (w/out git)
      to portage package names.

  """
  # Using a dictionary removes duplicates.
  revisions = {}
  for revision in revision_string.split():
    # Format 'package@commit-id'.
    revision_tuple = revision.split('@')
    if len(revision_tuple) != 2:
      Warning('Incorrectly formatted revision %s' % revision)

    repo_name = revision_tuple[0].replace('.git', '')
    # Might not have entry if no matching ebuild.
    if repo_dictionary.has_key(repo_name):
      # May be many corresponding packages to a given git repo e.g. kernel).
      for package in repo_dictionary[repo_name]:
        revisions[package] = revision_tuple[1]

  return revisions.items()


def _UprevFromRevisionList(buildroot, tracking_branch, revision_list, board,
                           overlays):
  """Uprevs based on revision list."""
  if not revision_list:
    Info('No packages found to uprev')
    return

  packages = []
  for package, revision in revision_list:
    assert ':' not in package, 'Invalid package name: %s' % package
    packages.append(package)

  chroot_overlays = [ReinterpretPathForChroot(path) for path in overlays]

  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['../../chromite/buildbot/cros_mark_as_stable',
              '--board=%s' % board,
              '--tracking_branch=%s' % tracking_branch,
              '--overlays=%s' % ':'.join(chroot_overlays),
              '--packages=%s' % ':'.join(packages),
              '--drop_file=%s' % ReinterpretPathForChroot(_PACKAGE_FILE %
                  {'buildroot': buildroot}),
              'commit'],
             cwd=cwd, enter_chroot=True)


def UprevPackages(buildroot, tracking_branch, revisionfile, board, overlays):
  """Uprevs a package based on given revisionfile.

  If revisionfile is set to None or does not resolve to an actual file, this
  function will uprev all packages.

  Keyword arguments:
  revisionfile -- string specifying a file that contains a list of revisions to
      uprev.
  """
  # Purposefully set to None as it means Force Build was pressed.
  revisions = 'None'
  if (revisionfile):
    try:
      rev_file = open(revisionfile)
      revisions = rev_file.read()
      rev_file.close()
    except Exception, e:
      Warning('Error reading %s, revving all' % revisionfile)
      revisions = 'None'

  revisions = revisions.strip()

  # revisions == "None" indicates a Force Build.
  if revisions != 'None':
    print >> sys.stderr, 'CBUILDBOT Revision list found %s' % revisions
    revision_list = _ParseRevisionString(revisions,
        _CreateRepoDictionary(buildroot, board))
    _UprevFromRevisionList(buildroot, tracking_branch, revision_list, board,
                           overlays)
  else:
    Info('CBUILDBOT Revving all')
    _UprevAllPackages(buildroot, tracking_branch, board, overlays)

