# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import posixpath

from path_util import SplitParent


def _SimplifyFileName(file_name):
  return (posixpath.splitext(file_name)[0]
      .lower()
      .replace('.', '')
      .replace('-', '')
      .replace('_', ''))


class PathCanonicalizer(object):
  '''Transforms paths into their canonical forms. Since the dev server has had
  many incarnations - e.g. there didn't use to be apps/ - there may be old
  paths lying around the webs. We try to redirect those to where they are now.
  '''
  def __init__(self, compiled_fs_factory, file_system):
    self._file_system = file_system
    # A lazily populated mapping of file names to a list of full paths that
    # contain them.  For example,
    #  - browserAction.html: [extensions/browserAction.html]
    #  - storage.html: [apps/storage.html, extensions/storage.html]
    self._files_to_paths = None

  def _GetPotentialPaths(self, filename):
    '''Returns the paths to any file called |filename|.
    '''
    if self._files_to_paths is None:
      self._files_to_paths = defaultdict(lambda: [])
      for base, dirs, files in self._file_system.Walk(''):
        for f in dirs + files:
          self._files_to_paths[_SimplifyFileName(f)].append(
              posixpath.join(base, f))
    return self._files_to_paths.get(_SimplifyFileName(filename))

  def Canonicalize(self, path):
    '''Returns the canonical path for |path|.
    '''
    # Path may already be the canonical path.
    if self._file_system.Exists(path).Get():
      return path

    # Path not found. Our single heuristic: find |basename| in the directory
    # structure with the longest common prefix of |path|.
    _, base = SplitParent(path)
    potential_paths = self._GetPotentialPaths(base)
    if not potential_paths:
      # There is no file with that name.
      return path

    # The most likely canonical file is the one with the longest common prefix.
    # This is slightly weaker than it could be; |path| is compared, not the
    # simplified form of |path|, which may matter.
    max_prefix = potential_paths[0]
    max_prefix_length = len(posixpath.commonprefix((max_prefix, path)))
    for path_for_file in potential_paths[1:]:
      prefix_length = len(posixpath.commonprefix((path_for_file, path)))
      if prefix_length > max_prefix_length:
        max_prefix, max_prefix_length = path_for_file, prefix_length

    return max_prefix
