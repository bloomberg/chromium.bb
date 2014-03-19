#!/usr/bin/python
# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for package info."""

import json
import os
import unittest
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '../..'))
import pynacl.working_directory

import package_info


class TestPackageInfo(unittest.TestCase):

  def test_HashEmptyForMissingFiles(self):
    # Many scripts rely on the archive hash returning None for missing files.
    with pynacl.working_directory.TemporaryWorkingDirectory() as work_dir:
      self.assertEqual(None, package_info.GetArchiveHash('missingfile.tgz'))

  def test_ArchiveHashStable(self):
    # Check if archive hash produces a stable hash
    with pynacl.working_directory.TemporaryWorkingDirectory() as work_dir:
      temp1 = os.path.join(work_dir, 'temp1.txt')
      temp2 = os.path.join(work_dir, 'temp2.txt')

      temp_contents = 'this is a test'
      with open(temp1, 'wt') as f:
        f.write(temp_contents)
      with open(temp2, 'wt') as f:
        f.write(temp_contents)

      self.assertEqual(package_info.GetArchiveHash(temp1),
                       package_info.GetArchiveHash(temp2))

  def test_AddArchive(self):
    # Check that we can successfully add an archive.
    archive_name = 'test_archive'
    archive_hash = 'test_archive_hash'
    archive_url = 'test_archive_url'
    tar_src_dir = 'test_archive_dir'
    extract_dir = 'test_extraction_dir'

    package = package_info.PackageInfo()
    package.AppendArchive(archive_name, archive_hash, archive_url,
                          tar_src_dir, extract_dir)
    archive_list = package.GetArchiveList()

    self.assertEqual(len(archive_list), 1)
    archive_item = archive_list[0]
    self.assertEqual(archive_item.name, archive_name)
    self.assertEqual(archive_item.hash, archive_hash)
    self.assertEqual(archive_item.url, archive_url)
    self.assertEqual(archive_item.tar_src_dir, tar_src_dir)
    self.assertEqual(archive_item.extract_dir, extract_dir)

  def test_ClearArchiveListClearsEverything(self):
    # Check that clear archive list actually clears everything
    package1 = package_info.PackageInfo()
    package1.AppendArchive('name', 'hash', 'url', 'tar_src_dir', 'extract_dir')
    package1.ClearArchiveList()

    # Test to be sure the archive list is clear.
    self.assertEqual(len(package1.GetArchiveList()), 0)

    # Test to be sure the state is equal to an empty PackageInfo.
    package2 = package_info.PackageInfo()
    self.assertEqual(package1, package2)

  def test_OrderIndependentEquality(self):
    # Check that order does not matter when adding multiple archives.
    archive_name1 = 'archive1.tar'
    archive_name2 = 'archive2.tar'
    archive_hash1 = 'archive_hash1'
    archive_hash2 = 'archive_hash2'

    package1 = package_info.PackageInfo()
    package1.AppendArchive(archive_name1, archive_hash1)
    package1.AppendArchive(archive_name2, archive_hash2)

    package2 = package_info.PackageInfo()
    package2.AppendArchive(archive_name2, archive_hash2)
    package2.AppendArchive(archive_name1, archive_hash1)

    self.assertEqual(len(package1.GetArchiveList()), 2)
    self.assertEqual(len(package2.GetArchiveList()), 2)
    self.assertEqual(package1, package2)

  def test_PackageLoadArchiveList(self):
    # Check if we can successfully load a list of archive.
    archive_name1 = 'archive_item1.tar'
    archive_name2 = 'archive_item2.tar'
    archive_hash1 = 'archive_item_hash1'
    archive_hash2 = 'archive_item_hash2'

    master_package = package_info.PackageInfo()
    master_package.AppendArchive(archive_name1, archive_hash1)
    master_package.AppendArchive(archive_name2, archive_hash2)
    archive_list = master_package.GetArchiveList()

    constructed_package = package_info.PackageInfo(archive_list)
    loaded_package = package_info.PackageInfo()
    loaded_package.LoadPackageFile(archive_list)

    self.assertEqual(master_package, constructed_package)
    self.assertEqual(master_package, loaded_package)

  def test_PackageLoadJsonList(self):
    # Check if we can successfully load a list of archive.
    archive_name1 = 'archive_item1.tar'
    archive_name2 = 'archive_item2.tar'
    archive_hash1 = 'archive_item_hash1'
    archive_hash2 = 'archive_item_hash2'

    master_package = package_info.PackageInfo()
    master_package.AppendArchive(archive_name1, archive_hash1)
    master_package.AppendArchive(archive_name2, archive_hash2)
    json_list = master_package.DumpPackageJson()

    constructed_package = package_info.PackageInfo(json_list)
    loaded_package = package_info.PackageInfo()
    loaded_package.LoadPackageFile(json_list)

    self.assertEqual(master_package, constructed_package)
    self.assertEqual(master_package, loaded_package)

  def test_PackageSaveLoadFile(self):
    # Check if we can save/load a package file and retain it's values.
    archive_name1 = 'archive_saveload1.tar'
    archive_name2 = 'archive_saveload2.tar'
    archive_hash1 = 'archive_saveload_hash1'
    archive_hash2 = 'archive_saveload_hash2'

    master_package = package_info.PackageInfo()
    master_package.AppendArchive(archive_name1, archive_hash1)
    master_package.AppendArchive(archive_name2, archive_hash2)

    with pynacl.working_directory.TemporaryWorkingDirectory() as work_dir:
      package_file = os.path.join(work_dir, 'test_package.json')
      master_package.SavePackageFile(package_file)

      constructed_package = package_info.PackageInfo(package_file)
      loaded_package = package_info.PackageInfo()
      loaded_package.LoadPackageFile(package_file)

      self.assertEqual(master_package, constructed_package)
      self.assertEqual(master_package, loaded_package)

  def test_PackageJsonDumpLoad(self):
    # Check if Json from DumpPackageJson() represents a package correctly.
    archive_name1 = 'archive_json1.tar'
    archive_name2 = 'archive_json2.tar'
    archive_hash1 = 'archive_json_hash1'
    archive_hash2 = 'archive_json_hash2'

    master_package = package_info.PackageInfo()
    master_package.AppendArchive(archive_name1, archive_hash1)
    master_package.AppendArchive(archive_name2, archive_hash2)
    with pynacl.working_directory.TemporaryWorkingDirectory() as work_dir:
      temp_json = os.path.join(work_dir, 'temp.json')
      with open(temp_json, 'wt') as f:
        json.dump(master_package.DumpPackageJson(), f)
      json_package = package_info.PackageInfo(temp_json)

      self.assertEqual(master_package, json_package)


if __name__ == '__main__':
  unittest.main()
