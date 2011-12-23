#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Functionality for mangling repository checkouts that are shared

In particular, this in combination w/ enter_chroot's mount binding, allows
us to access the same repo from inside and outside a chroot at the same time
"""

__all__ = ('RebuildRepoCheckout',)

import os
import shutil
import errno

import cros_build_lib


# TODO(ferringb): either move _atomic_writefile, _safe_makedirs, and
# _safe_unlink into a library, or refactor it to use snakeoil; do this
# once the question of whether or not to use snakeoil is answered.

def _atomic_writefile(path, data):
  """Write to a tmpfile, than use rename to ensure nothing see partial data"""

  tmp = path + '.tmp'

  with open(tmp, 'w') as f:
    f.write(data)

  try:
    os.rename(tmp, path)
  except EnvironmentError:

    _safe_unlink(tmp, True)

    raise


def _safe_makedirs(path, mode=0775):
  try:
    os.makedirs(path, mode)
  except EnvironmentError as e:
    if e.errno != errno.EEXIST:
      raise


def _safe_unlink(path, suppress=False):
  try:
    os.unlink(path)
  except EnvironmentError, e:
    if not suppress or e.errno != errno.ENOENT:
      raise


def _FilterNonExistentProjects(real_reference, projects):
  for project in projects:
    if os.path.exists(os.path.join(real_reference, project)):
      yield project


def _CleanAlternates(projects, alt_root):
  alt_root = os.path.normpath(alt_root)

  projects = set(projects)
  required_directories = set(os.path.dirname(x) for x in projects)

  for abs_root, dirs, files in os.walk(alt_root):
    rel_root = abs_root[len(alt_root):].strip('/')

    if rel_root not in required_directories:
      shutil.rmtree(abs_root)
      dirs[:] = []
      continue

    if rel_root:
      for filename in files:
        if os.path.join(rel_root, filename) not in projects:
          os.unlink(os.path.join(abs_root, filename))


def _UpdateAlternatesDir(alternates_root, reference_maps, projects):
  for project in projects:
    alt_path = os.path.join(alternates_root, project)
    paths = []
    for k, v in reference_maps.iteritems():
      k = os.path.join(k, '.repo', 'projects', project, 'objects')
      if os.path.exists(k):
        paths.append(os.path.join(v, '.repo', 'projects', project, 'objects'))

    _safe_makedirs(os.path.dirname(alt_path))

    _atomic_writefile(alt_path, '%s\n' % ('\n'.join(paths),))


def _UpdateGitAlternates(proj_root, projects):
  for project in projects:

    alt_path = os.path.join(proj_root, project, 'objects', 'info',
      'alternates')
    tmp_path = '%s.tmp' % alt_path

    # Clean out any tmp files that may have existed prior.
    _safe_unlink(tmp_path, True)

    # The pathway is written relative to the alternates files absolute path;
    # literally, .repo/projects/chromite.git/objects/info/alternates.
    relpath = '../' * (project.count('/') + 4)
    relpath = os.path.join(relpath, 'alternates', project)

    _safe_makedirs(os.path.dirname(tmp_path))
    os.symlink(relpath , tmp_path)
    os.rename(tmp_path, alt_path)


class Failed(Exception):
  pass


def _RebuildRepoCheckout(target_root, reference_map,
                         alternates_dir):
  repo_root = os.path.join(target_root, '.repo')
  proj_root = os.path.join(repo_root, 'projects')

  if not os.path.exists(repo_root):
    raise Failed('%r does not exist, thus cannot be a repo checkout' %
                 repo_root)
  try:
    with open(os.path.join(repo_root, 'project.list')) as f:
      project_data = f.read()
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise
    project_data = ''

  projects = list(sorted('%s.git' % x for x in project_data.split()))
  projects = list(_FilterNonExistentProjects(proj_root, projects))

  _safe_makedirs(alternates_dir)
  try:
    os.makedirs(alternates_dir, 0775)
  except EnvironmentError, e:
    if e.errno != errno.EEXIST:
      raise

    # We know the directory exists; thus cleanse out
    # dead alternates.
    _CleanAlternates(projects, alternates_dir)

  _UpdateAlternatesDir(alternates_dir, reference_map, projects)
  _UpdateGitAlternates(proj_root, projects)


def WalkReferences(repo_root, max_depth=5):
  """
  Given a repo checkout root, find the repo's it references up to max_depth

  Args:
    repo_root: The root of a repo checkout to start from
    max_depth: Git internally limits the max alternates depth to 5;
      this option exists to adjust how deep we're willing to look.

  Returns:
    List of repository roots required for this repo_root.
  """

  original_root = repo_root
  yield repo_root

  cur_depth = max_depth
  while cur_depth:
    base = os.path.join(repo_root, '.repo', 'manifests.git')
    result = cros_build_lib.RunCommand(
        ['git', 'config', '--file', os.path.join(base, 'config'),
        'repo.reference'], print_cmd=False, redirect_stdout=True, cwd=base,
        error_ok=True)

    if result.returncode not in (0, 1):
      raise Failed('Unexpected returncode %i from examining %s git '
                   'repo.reference configuration' %
                   (result.returncode, base))

    repo_root = result.output.strip()
    if not repo_root:
      break

    yield repo_root
    cur_depth -= 1

  if not cur_depth:
    raise Failed('While tracing out the references of %s, we recursed more '
                 'than the allowed %i times ending at %s'
                 % (original_root, max_depth, repo_root))


def RebuildRepoCheckout(repo_root, initial_reference,
                        chroot_reference_root=None):
  """
  Rebuild a repo checkouts ondisk 'alternate tree' rewriting the repo to use it

  Args:
    repo_root: absolute path to the root of a repository checkout
    reference_root: absolute path to the root of the repository that is
      shared
    chroot_reference_root: if given, repo_root will have it's chroot
      alternates tree configured with this pathway, enabling repo access to
      work from within the chroot.
  """

  reference_roots = list(WalkReferences(initial_reference))
  if chroot_reference_root:
    alternates_dir = 'chroot/alternates'
    base = os.path.join(chroot_reference_root, '.repo', 'chroot', 'external')
    reference_map = dict((x, '%s%i' % (base, idx + 1))
                         for idx, x in enumerate(reference_roots))
  else:
    alternates_dir = 'alternates'
    reference_map = dict((x, x) for x in reference_roots)

  alternates_dir = os.path.join(repo_root, '.repo', alternates_dir)

  _RebuildRepoCheckout(repo_root,
                       reference_map,
                       alternates_dir)
  return reference_map


if __name__ == '__main__':
  import sys
  chroot_reference_root = None
  if len(sys.argv) not in (3, 4):
    sys.stderr.write('Wrong argument count; correct invocation is '
                     'repository_root referenced_repository '
                     '[path_from_within_the_chroot]\n')
    sys.exit(1)
  if len(sys.argv) == 4:
    chroot_reference_root = sys.argv[3]
  ret = RebuildRepoCheckout(sys.argv[1], sys.argv[2],
                            chroot_reference_root=chroot_reference_root)
  print '\n'.join(ret.iterkeys())
