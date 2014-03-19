#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A package is a json file describing a list of package archives."""

import collections
import hashlib
import json
import os


ArchiveInfo = collections.namedtuple(
    'ArchiveInfo',
    ['name', 'hash', 'url', 'tar_src_dir', 'extract_dir'])


def GetArchiveHash(archive_file):
  """Gets the standardized hash value for a given archive.

  This hash value is the expected value used to verify package archives.

   Args:
     archive_file: Path to archive file to hash.
   Returns:
     Hash value of archive file, or None if file is invalid.
  """
  if os.path.isfile(archive_file):
    with open(archive_file, 'rb') as f:
      return hashlib.sha1(f.read()).hexdigest()
  return None


class PackageInfo(object):
  """A package file is a list of package archives (usually .tar or .tgz files).

  Archive Fields:
    name: Name of the package archive.
    hash: Hash value of the package archive, for validation purposes.
    url: Web URL location where the archive can be found.
    tar_src_dir: Where files are located within the tar archive.
    extract_dir: Where files should be extracted to within destination dir.
  """
  def __init__(self, package_file=None):
    self._archive_list = []

    if package_file is not None:
      self.LoadPackageFile(package_file)

  def __eq__(self, other):
    return (type(self) == type(other) and
            set(self.GetArchiveList()) == set(other.GetArchiveList()))

  def __repr__(self):
    return "PackageInfo(archive_list=" + str(self._archive_list) + ")"

  def LoadPackageFile(self, package_file):
    """Loads a package file into this object.

    Args:
      package_file: Filename or list of ArchiveInfos.
    """
    package_json = None
    if isinstance(package_file, list):
      if package_file and isinstance(package_file[0], ArchiveInfo):
        self._archive_list = package_file
      else:
        package_json = package_file
    elif isinstance(package_file, basestring):
      with open(package_file, 'rt') as f:
        package_json = json.load(f)
    else:
      raise RuntimeError('Invalid load package file type (%s): %s',
                         type(package_file),
                         package_file)

    if package_json is not None:
      self._archive_list = [ArchiveInfo(**package) for package in package_json]

  def SavePackageFile(self, package_file):
    """Saves this object as a serialized JSON file.

    Args:
      package_file: File path where JSON file will be saved.
    """
    package_json = self.DumpPackageJson()
    with open(package_file, 'wt') as f:
      json.dump(package_json, f, sort_keys=True,
                indent=2, separators=(',', ': '))

  def DumpPackageJson(self):
    """Returns a list representation of the JSON of this object."""
    return [dict(package._asdict()) for package in self._archive_list]

  def ClearArchiveList(self):
    """Clears this object so it represents no archives."""
    self._archive_list = []

  def AppendArchive(self, name, archive_hash, url=None, tar_src_dir='',
                    extract_dir=''):
    """Append a package archive into this object"""
    archive_info = ArchiveInfo(name, archive_hash, url,
                               tar_src_dir, extract_dir)
    self._archive_list.append(archive_info)

  def GetArchiveList(self):
    """Returns the list of ARCHIVE_INFOs this object represents."""
    return self._archive_list
