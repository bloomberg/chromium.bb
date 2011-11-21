#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Class that contains paths classified into four types."""

import os


class Error(Exception):
  pass


class PathSet(object):
  '''Container for paths classified into four types.

  Paths in this container are broken into four separate collections, each
  accessible as a property (see the "Attributes" section, below).

  Attributes:
    files: A set of plain files, keys are platform-specific normalized path
        names.  Might be empty.
    dirs: A set of directories, keys are platform-specific normalized path
        names.  Might be empty.
    symlinks: A dictionary of symbolic links - these are not followed.  Keys
        are platform-specific normalized path names.  The value of each key is
        the source file of the link (also a platform-specific normalized path).
        Might be empty.
    links: A dictionary of hard links.  Keys are platform-specific normalized
        path names.  The value of each key is the source file of the link (also
        a platform-specific normalized path).  Might be empty.
  '''

  def __init__(self):
    self.Reset()

  def __ior__(self, other):
    '''Override |= to merge |other| into this path set.'''
    (self._files, self._dirs,
     self._symlinks, self._links) = self._MergeWithPathSet(other)
    return self

  def __or__(self, other):
    '''Override | to produce the merger of |self| and |other|.'''
    merged_path_set = PathSet()
    (merged_path_set.files,
     merged_path_set.dirs,
     merged_path_set.symlinks,
     merged_path_set.links) = self._MergeWithPathSet(other)
    return merged_path_set

  def _MergeWithPathSet(self, path_set):
    '''Merge this path set with |path_set|.

    Forms the union of the |_files| and |_dirs| sets, then merges the
    |_symlinks| and |_links| dicts.  The dictionaries are merged in such that
    keys are not duplicated: the values of the keys in |path_set| take
    precedence in the returned dictionaries.

    Any keys in either the symlink or links dictionaries that also exist in
    either of the files or dicts sets are removed from the latter, meaning that
    symlinks or links which overlap file or directory entries take precedence.

    Args:
      path_set: The other path set.  Must be an object with four properties:
          files (a set), dirs (a set), symlinks (a dict), links (a dict).
    Returns:
      A four-tuple (files, dirs, symlinks, links) which is the result of merging
      the two PathSets.
    '''
    def DiscardOverlappingLinks(links_dict):
      '''Discard all overlapping keys from files and dirs.'''
      for link in links_dict:
        self._files.discard(link)
        self._dirs.discard(link)

    DiscardOverlappingLinks(path_set.symlinks)
    DiscardOverlappingLinks(path_set.links)
    return (self._files | path_set.files,
            self._dirs | path_set.dirs,
            dict(self._symlinks.items() + path_set.symlinks.items()),
            dict(self._links.items() + path_set.links.items()))

  @property
  def files(self):
    '''The set of plain files.'''
    return self._files

  @files.setter
  def files(self, new_file_set):
    if isinstance(new_file_set, set):
      self._files = new_file_set
    else:
      raise Error('files property must be a set')

  @property
  def dirs(self):
    '''The set of directories.'''
    return self._dirs

  @dirs.setter
  def dirs(self, new_dir_set):
    if isinstance(new_dir_set, set):
      self._dirs = new_dir_set
    else:
      raise Error('dirs property must be a set')

  @property
  def symlinks(self):
    '''The dictionary of symbolic links.'''
    return self._symlinks

  @symlinks.setter
  def symlinks(self, new_symlinks_dict):
    if isinstance(new_symlinks_dict, dict):
      self._symlinks = new_symlinks_dict
    else:
      raise Error('symlinks property must be a dict')

  @property
  def links(self):
    '''The dictionary of hard links.'''
    return self._links

  @links.setter
  def links(self, new_links_dict):
    if isinstance(new_links_dict, dict):
      self._links = new_links_dict
    else:
      raise Error('links property must be a dict')

  def Reset(self):
    '''Reset all attributes to empty containers.'''
    self._files = set()
    self._dirs = set()
    self._symlinks = dict()
    self._links = dict()

  def PrependPath(self, path_prefix):
    '''Prepend paths in all collections with |path_prefix|.

    All the keys in all the colletions get prepended with |path_prefix|.  The
    resulting path is an os-specific path.

    Args:
      path_prefix: The path to prepend to all collection keys.
    '''
    prepend_path = lambda p: os.path.join(path_prefix, p)
    def PrependToLinkDict(link_dict):
      return dict([prepend_path(p), link] for p, link in link_dict.items())

    self._files = set(map(prepend_path, self._files))
    self._dirs = set(map(prepend_path, self._dirs))
    self._symlinks = PrependToLinkDict(self._symlinks)
    self._links = PrependToLinkDict(self._links)
