#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A archive_info is a json file describing a single package archive."""

import collections
import hashlib
import json
import os


ArchiveInfoTuple = collections.namedtuple(
    'ArchiveInfoTuple',
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


class ArchiveInfo(object):
  """A archive_info file is a single json file describine an archive.

  Archive Fields:
    name: Name of the package archive.
    hash: Hash value of the package archive, for validation purposes.
    url: Web URL location where the archive can be found.
    tar_src_dir: Where files are located within the tar archive.
    extract_dir: Where files should be extracted to within destination dir.
  """
  def __init__(self, name='', archive_hash=0, url=None, tar_src_dir='',
               extract_dir='', archive_info_file=None):
    """Initialize ArchiveInfo object.

    When an archive_info_file is specified, all other fields are ignored.
    Otherwise, uses first fields as constructor for archive info object.
    """
    self._archive_tuple = None

    if archive_info_file is not None:
      self.LoadArchiveInfoFile(archive_info_file)
    else:
      self.SetArchiveData(name, archive_hash, url, tar_src_dir, extract_dir)

  def __eq__(self, other):
    return (type(self) == type(other) and
            self.GetArchiveData() == other.GetArchiveData())

  def __repr__(self):
    return "ArchiveInfo(" + str(self._archive_tuple) + ")"

  def LoadArchiveInfoFile(self, archive_info_file):
    """Loads a archive info file into this object.

    Args:
      archive_info_file: Filename or archive info json.
    """
    archive_json = None
    if isinstance(archive_info_file, dict):
      archive_json = archive_info_file
    elif isinstance(archive_info_file, basestring):
      with open(archive_info_file, 'rt') as f:
        archive_json = json.load(f)
    else:
      raise RuntimeError('Invalid load archive file type (%s): %s',
                         type(archive_info_file),
                         archive_info_file)

    self._archive_tuple = ArchiveInfoTuple(**archive_json)

  def SaveArchiveInfoFile(self, archive_info_file):
    """Saves this object as a serialized JSON file.

    Args:
      archive_info_file: File path where JSON file will be saved.
    """
    archive_json = self.DumpArchiveJson()
    with open(archive_info_file, 'wt') as f:
      json.dump(archive_json, f, sort_keys=True,
                indent=2, separators=(',', ': '))

  def DumpArchiveJson(self):
    """Returns a dict representation of this object for JSON."""
    if self._archive_tuple is None:
      return {}

    return dict(self._archive_tuple._asdict())

  def SetArchiveData(self, name, archive_hash, url=None, tar_src_dir='',
                     extract_dir=''):
    """Replaces currently set with new ArchiveInfoTuple."""
    self._archive_tuple = ArchiveInfoTuple(name, archive_hash, url,
                                           tar_src_dir, extract_dir)

  def GetArchiveData(self):
    """Returns the current ArchiveInfoTuple tuple."""
    return self._archive_tuple
