#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This library keeps track of all the standard locations for package files"""

import os
import posixpath


def GetRemotePackageKey(is_shared, rev_num, package_target, package_name):
  """Returns key for package files in the google storage cloud.

  Args:
    is_shared: Whether or not the package is marked as shared.
    rev_num: The SVN revision number of when the package was built.
    package_target: The package target which this package belongs to.
    package_name: The name of the package.
  Returns:
    The google cloud storage key where the package file should be found.
  """
  if is_shared:
    intermediate_dir = SHARED_FOLDER
  else:
    intermediate_dir = package_target

  return posixpath.join(
      'builds',
      str(rev_num),
      intermediate_dir,
      package_name + '.json'
  )


def GetRemotePackageArchiveKey(archive_name, hash_value):
  """Returns key for package archive files in the google storage cloud.

  Args:
    archive_name: The name of the archive.
    hash_value: The hash of the archive.
  Returns:
    The google cloud storage key where the package archive should be found.
  """
  return posixpath.join(
      'archives',
      archive_name,
      hash_value
  )


def GetLocalPackageFile(tar_dir, package_target, package_name):
  """Returns the local package file location.

  Args:
    tar_dir: The tar directory for where package archives would be found.
    package_target: The package target of the package.
    package_name: The name of the package.
  Returns:
    The standard location where local package file is found.
  """
  return os.path.join(
      tar_dir,
      package_target,
      package_name + '.json'
  )


def GetLocalPackageArchiveFile(tar_dir, package_target, package_name,
                               archive_name):
  """Returns the local package archive location.

  Args:
    tar_dir: The tar directory for where package archives would be found.
    package_target: The package target of the package containing the archive.
    package_name: The name of the package containing the archive.
    archive_name: The name of the archive contained within the package.
  Returns:
    The standard location where local package archive file is found.
  """
  return os.path.join(
      tar_dir,
      package_target,
      archive_name
  )


def GetRevisionFile(revision_dir, package_name):
  """Returns the local revision file location.

  Args:
    revision_dir: Revision directory where revision files should be found.
    package_name: The name of the package revision file represents.
  Returns:
    The standard location where the revision file is found.
  """
  return os.path.join(
      revision_dir,
      package_name + '.json'
  )


def GetFullDestDir(dest_dir, package_target, package_name):
  """Returns the destination directory for a package archive.

  Args:
    dest_dir: Destination directory root.
    package_target: Package target of the package to extract.
    package_name: The package name of the package to extract.
  Returns:
    The package directory within the destination directory.
  """
  return os.path.join(dest_dir, package_target, package_name)


def GetDestPackageFile(dest_dir, package_target, package_name):
  """Returns the package file stored in the destination directory.

  The package file is normally stored inside of the tar directory, but upon
  extraction a copy of the package file is also made into the extraction
  directory for book keeping purposes.

  Args:
    dest_dir: Destination directory root.
    package_target: Package target of the package to extract.
    package_name: The package name of the package to extract.
  Returns:
    The location of the package file within the destination directory.
  """

  return os.path.join(
      GetFullDestDir(dest_dir, package_target, package_name),
      package_name + '.json'
  )
