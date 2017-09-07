// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/common/fileapi/file_system_mount_option.h"

using base::Bind;
using storage::ExternalMountPoints;

namespace {

static base::LazyInstance<ImportedMediaGalleryRegistry>::Leaky
g_imported_media_gallery_registry = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ImportedMediaGalleryRegistry* ImportedMediaGalleryRegistry::GetInstance() {
  return g_imported_media_gallery_registry.Pointer();
}

void ImportedMediaGalleryRegistry::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();
  if (imported_root_.empty()) {
    if (!base::CreateTemporaryFile(&imported_root_))
      imported_root_ = base::FilePath();
    // TODO(vandebo) Setting the permissions of |imported_root_| in CPSP to
    // zero would be an extra step to ensure permissions are correctly
    // enforced.
  }
}

bool ImportedMediaGalleryRegistry::RegisterITunesFilesystemOnUIThread(
    const std::string& fs_name, const base::FilePath& library_xml_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!library_xml_path.empty());

  bool result = false;

#if defined(OS_WIN) || defined(OS_MACOSX)
  base::FilePath root = ImportedRoot();
  if (root.empty())
    return false;
  result = ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      fs_name,
      storage::kFileSystemTypeItunes,
      storage::FileSystemMountOption(),
      root.AppendASCII("itunes"));
  if (!result)
    return result;

  itunes_fs_names_.insert(fs_name);

  if (itunes_fs_names_.size() == 1) {
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        Bind(&ImportedMediaGalleryRegistry::RegisterITunesFileSystem,
             base::Unretained(this), library_xml_path));
#ifndef NDEBUG
    itunes_xml_library_path_ = library_xml_path;
  } else {
    DCHECK_EQ(itunes_xml_library_path_.value(), library_xml_path.value());
#endif
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  return result;
}

bool ImportedMediaGalleryRegistry::RevokeImportedFilesystemOnUIThread(
    const std::string& fs_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

#if defined(OS_WIN) || defined(OS_MACOSX)
  if (itunes_fs_names_.erase(fs_name)) {
    if (itunes_fs_names_.empty()) {
      MediaFileSystemBackend::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokeITunesFileSystem,
               base::Unretained(this)));
    }
    return ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(fs_name);
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  return false;
}

base::FilePath ImportedMediaGalleryRegistry::ImportedRoot() {
  DCHECK(!imported_root_.empty());
  return imported_root_;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// static
itunes::ITunesDataProvider*
ImportedMediaGalleryRegistry::ITunesDataProvider() {
  MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
  DCHECK(GetInstance()->itunes_data_provider_);
  return GetInstance()->itunes_data_provider_.get();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

ImportedMediaGalleryRegistry::ImportedMediaGalleryRegistry() {}

ImportedMediaGalleryRegistry::~ImportedMediaGalleryRegistry() {
  if (!imported_root_.empty())
    base::DeleteFile(imported_root_, false);
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_EQ(0U, itunes_fs_names_.size());
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void ImportedMediaGalleryRegistry::RegisterITunesFileSystem(
    const base::FilePath& xml_library_path) {
  MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
  DCHECK(!itunes_data_provider_);
  itunes_data_provider_.reset(new itunes::ITunesDataProvider(xml_library_path));
}

void ImportedMediaGalleryRegistry::RevokeITunesFileSystem() {
  MediaFileSystemBackend::AssertCurrentlyOnMediaSequence();
  DCHECK(itunes_data_provider_);
  itunes_data_provider_.reset();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

