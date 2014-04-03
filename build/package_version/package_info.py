#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A package is a json file describing a list of package archives."""

import json
import os
import posixpath
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
import pynacl.file_tools
import pynacl.gsd_storage

import archive_info


def ReadPackageFile(package_file):
  """Returns a PackageInfoTuple representation of a json package file."""
  with open(package_file, 'rt') as f:
    return json.load(f)


def GetFileBaseName(filename):
  """Removes all extensions from a file.

  (Note: os.path.splitext() only removes the last extension).
  """
  first_ext = filename.find('.')
  if first_ext != -1:
    filename = filename[:first_ext]
  return filename


def GetLocalPackageName(local_package_file):
  """Returns the package name given a local package file."""
  package_basename = os.path.basename(local_package_file)
  return GetFileBaseName(package_basename)


def GetRemotePackageName(remote_package_file):
  """Returns the package name given a remote posix based package file."""
  package_basename = posixpath.basename(remote_package_file)
  return GetFileBaseName(package_basename)


def DownloadPackageInfoFiles(local_package_file, remote_package_file,
                             downloader=None):
  """Downloads all package info files from a downloader.

  Downloads a package file from the cloud along with all of the archive
  info files. Archive info files are expected to be in a directory with the
  name of the package along side the package file. Files will be downloaded
  in the same structure.

  Args:
    local_package_file: Local package file where root file will live.
    remote_package_file: Remote package URL to download from.
    downloader: Optional downloader if standard HTTP one should not be used.
  """
  if downloader is None:
    downloader = pynacl.gsd_storage.HttpDownload

  pynacl.file_tools.MakeParentDirectoryIfAbsent(local_package_file)
  downloader(remote_package_file, local_package_file)

  archive_list = ReadPackageFile(local_package_file)
  local_package_name = GetLocalPackageName(local_package_file)
  remote_package_name = GetRemotePackageName(remote_package_file)

  local_archive_dir = os.path.join(os.path.dirname(local_package_file),
                                   local_package_name)
  remote_archive_dir = posixpath.join(posixpath.dirname(remote_package_file),
                                      remote_package_name)

  pynacl.file_tools.MakeDirectoryIfAbsent(local_archive_dir)
  for archive in archive_list:
    archive_file = archive + '.json'
    local_archive_file = os.path.join(local_archive_dir, archive_file)
    remote_archive_file = posixpath.join(remote_archive_dir, archive_file)
    downloader(remote_archive_file, local_archive_file)
    if not os.path.isfile(local_archive_file):
      raise IOError('Could not download archive file: %s.' %
                    remote_archive_file)

def UploadPackageInfoFiles(storage, package_target, package_name,
                           remote_package_file, local_package_file,
                           skip_missing=False, annotate=False):
  """Uploads all package info files from a downloader.

  Uploads a package file to the cloud along with all of the archive info
  files. Archive info files are expected to be in a directory with the
  name of the package along side the package file. Files will be uploaded
  using the same file structure.

  Args:
    storage: Cloud storage object to store the files to.
    remote_package_file: Remote package URL to upload to.
    local_package_file: Local package file where root file lives.
    skip_missing: Whether to skip missing archive files or error.
    annotate: Whether to annotate build bot links.
  Returns:
    The URL where the root package file is located.
  """
  archive_list = ReadPackageFile(local_package_file)
  local_package_name = GetLocalPackageName(local_package_file)
  remote_package_name = GetRemotePackageName(remote_package_file)

  local_archive_dir = os.path.join(os.path.dirname(local_package_file),
                                   local_package_name)
  remote_archive_dir = posixpath.join(posixpath.dirname(remote_package_file),
                                      remote_package_name)

  for index, archive in enumerate(archive_list):
    archive_file = archive + '.json'
    local_archive_file = os.path.join(local_archive_dir, archive_file)
    remote_archive_file = posixpath.join(remote_archive_dir, archive_file)
    if skip_missing and not os.path.isfile(local_archive_file):
      continue

    if annotate:
      print ('@@@BUILD_STEP %s:%s:Archive[%d] (upload)@@@' %
             (package_target, package_name, index))

    archive_url = storage.PutFile(local_archive_file, remote_archive_file)
    if annotate:
      print '@@@STEP_LINK@download@%s@@@' % archive_url

  if annotate:
    print '@@@BUILD_STEP %s:%s (upload)@@@' % (package_target, package_name)
  package_url = storage.PutFile(local_package_file, remote_package_file)
  if annotate:
    print '@@@STEP_LINK@download@%s@@@' % package_url
  return package_url


class PackageInfo(object):
  """A package file is a list of package archives (usually .tar or .tgz files).

  PackageInfo will contain a list of ArchiveInfo objects, ArchiveInfo will
  contain all the necessary information for an archive (name, URL, hash...etc.).
  """
  def __init__(self, package_file=None, skip_missing=False):
    self._archive_list = []

    if package_file is not None:
      self.LoadPackageFile(package_file, skip_missing)

  def __eq__(self, other):
    if type(self) != type(other):
      return False

    archives1 = [archive.GetArchiveData() for archive in self.GetArchiveList()]
    archives2 = [archive.GetArchiveData() for archive in other.GetArchiveList()]
    return set(archives1) == set(archives2)

  def __repr__(self):
    return "PackageInfo(archive_list=" + str(self.GetArchiveList()) + ")"

  def LoadPackageFile(self, package_file, skip_missing=False):
    """Loads a package file into this object.

    Args:
      package_file: Filename or list of archives in json format.
    """
    archive_names = None
    self._archive_list = []

    if isinstance(package_file, list):
      if package_file:
        if isinstance(package_file[0], archive_info.ArchiveInfo):
          # Setting a list of ArchiveInfo objects, no need to interpret JSON.
          self._archive_list = package_file
        else:
          # Assume to be JSON.
          for archive_json in package_file:
            archive = archive_info.ArchiveInfo(archive_info_file=archive_json)
            self._archive_list.append(archive)

    elif isinstance(package_file, basestring):
      archive_names = ReadPackageFile(package_file)
      package_name = GetLocalPackageName(package_file)
      archive_dir = os.path.join(os.path.dirname(package_file), package_name)
      for archive in archive_names:
        arch_file = archive + '.json'
        arch_path = os.path.join(archive_dir, arch_file)
        if not os.path.isfile(arch_path):
          if not skip_missing:
            raise RuntimeError(
                'Package (%s) points to invalid archive file (%s).' %
                (package_file, arch_path))
          archive_desc = archive_info.ArchiveInfo(archive)
        else:
          archive_desc = archive_info.ArchiveInfo(archive_info_file=arch_path)
        self._archive_list.append(archive_desc)
    else:
      raise RuntimeError('Invalid load package file type (%s): %s',
                         (type(package_file), package_file))

  def SavePackageFile(self, package_file):
    """Saves this object as a serialized JSON file.

    Args:
      package_file: File path where JSON file will be saved.
    """
    package_name = GetLocalPackageName(package_file)
    archive_dir = os.path.join(os.path.dirname(package_file), package_name)
    pynacl.file_tools.MakeDirectoryIfAbsent(archive_dir)

    package_json = []

    for archive in self.GetArchiveList():
      archive_data = archive.GetArchiveData()
      package_json.append(archive_data.name)

      archive_file = archive_data.name + '.json'
      archive_path = os.path.join(archive_dir, archive_file)
      archive.SaveArchiveInfoFile(archive_path)

    with open(package_file, 'wt') as f:
      json.dump(package_json, f, sort_keys=True,
                indent=2, separators=(',', ': '))

  def DumpPackageJson(self):
    """Returns a list representation of the JSON of this object."""
    return [archive.DumpArchiveJson() for archive in self.GetArchiveList()]

  def ClearArchiveList(self):
    """Clears this object so it represents no archives."""
    self._archive_list = []

  def AppendArchive(self, archive_info):
    """Append a package archive into this object"""
    self._archive_list.append(archive_info)

  def GetArchiveList(self):
    """Returns the sorted list of ARCHIVE_INFOs this object represents."""
    return sorted(self._archive_list,
                  key=lambda archive : archive.GetArchiveData().name)
