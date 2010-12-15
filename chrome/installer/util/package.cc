// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/package.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/win/registry.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;

namespace installer {

Package::Package(bool system_level, const FilePath& path,
                 PackageProperties* properties)
    : system_level_(system_level), path_(path), properties_(properties) {
  DCHECK(properties_);
}

Package::~Package() {
}

const FilePath& Package::path() const {
  return path_;
}

const Products& Package::products() const {
  return products_;
}

PackageProperties* Package::properties() const {
  return properties_;
}

bool Package::IsEqual(const FilePath& path) const {
  return FilePath::CompareEqualIgnoreCase(path_.value(), path.value());
}

void Package::AssociateProduct(const Product* product) {
#ifndef NDEBUG
  for (size_t i = 0; i < products_.size(); ++i) {
    DCHECK_NE(product->distribution()->GetType(),
              products_[i]->distribution()->GetType());
  }
#endif
  products_.push_back(product);
}

bool Package::system_level() const {
  return system_level_;
}

FilePath Package::GetInstallerDirectory(
    const Version& version) const {
  return path_.Append(version.GetString())
      .Append(installer::kInstallerDir);
}

Version* Package::GetCurrentVersion() const {
  scoped_ptr<Version> current_version;
  // Be aware that there might be a pending "new_chrome.exe" already in the
  // installation path.
  FilePath new_chrome_exe(path_.Append(installer::kChromeNewExe));
  bool new_chrome_exists = file_util::PathExists(new_chrome_exe);

  HKEY root = system_level_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  for (size_t i = 0; i < products_.size(); ++i) {
    const Product* product = products_[i];
    RegKey chrome_key(root, product->distribution()->GetVersionKey().c_str(),
                      KEY_READ);
    std::wstring version;
    if (new_chrome_exists)
      chrome_key.ReadValue(google_update::kRegOldVersionField, &version);

    if (version.empty())
      chrome_key.ReadValue(google_update::kRegVersionField, &version);

    if (!version.empty()) {
      scoped_ptr<Version> this_version(Version::GetVersionFromString(version));
      if (this_version.get()) {
        if (!current_version.get() ||
            current_version->IsHigherThan(this_version.get())) {
          current_version.reset(this_version.release());
        } else if (current_version.get()) {
          DCHECK_EQ(current_version->GetString(), this_version->GetString())
              << "found distributions of different versions in the same "
                 "installation folder!";
        }
      }
    }
  }

  return current_version.release();
}

void Package::RemoveOldVersionDirectories(
    const Version& latest_version) const {
  std::wstring search_path(path_.value());
  file_util::AppendToPath(&search_path, L"*");

  // TODO(tommi): use file_util::FileEnumerator.
  WIN32_FIND_DATA find_file_data;
  HANDLE file_handle = FindFirstFile(search_path.c_str(), &find_file_data);
  if (file_handle == INVALID_HANDLE_VALUE) {
    VLOG(1) << "No directories found under: " << search_path;
    return;
  }

  BOOL ret = TRUE;
  scoped_ptr<Version> version;

  // We try to delete all directories whose versions are lower than
  // latest_version.
  while (ret) {
    if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY &&
        lstrcmpW(find_file_data.cFileName, L"..") != 0 &&
        lstrcmpW(find_file_data.cFileName, L".") != 0) {
      VLOG(1) << "directory found: " << find_file_data.cFileName;
      version.reset(Version::GetVersionFromString(find_file_data.cFileName));
      if (version.get() && latest_version.IsHigherThan(version.get())) {
        FilePath remove_dir(path_.Append(find_file_data.cFileName));
        std::vector<FilePath> key_files;

        Products::const_iterator it = products_.begin();
        for (; it != products_.end(); ++it) {
          BrowserDistribution* dist = it->get()->distribution();
          std::vector<FilePath> dist_key_files(dist->GetKeyFiles());
          std::vector<FilePath>::const_iterator key_file_iter(
              dist_key_files.begin());
          for (; key_file_iter != dist_key_files.end(); ++key_file_iter) {
            key_files.push_back(remove_dir.Append(*key_file_iter));
          }
        }

        VLOG(1) << "Deleting directory: " << remove_dir.value();

        scoped_ptr<DeleteTreeWorkItem> item(
            WorkItem::CreateDeleteTreeWorkItem(remove_dir, key_files));
        if (!item->Do())
          item->Rollback();
      }
    }
    ret = FindNextFile(file_handle, &find_file_data);
  }

  FindClose(file_handle);
}

}  // namespace installer

