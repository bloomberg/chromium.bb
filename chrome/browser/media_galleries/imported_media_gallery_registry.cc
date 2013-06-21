// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_data_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/isolated_context.h"

using base::Bind;

namespace chrome {

namespace {

static base::LazyInstance<ImportedMediaGalleryRegistry>::Leaky
g_imported_media_gallery_registry = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ImportedMediaGalleryRegistry* ImportedMediaGalleryRegistry::GetInstance() {
  return g_imported_media_gallery_registry.Pointer();
}

std::string ImportedMediaGalleryRegistry::RegisterPicasaFilesystemOnUIThread(
    const base::FilePath& database_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!database_path.empty());

  std::string fsid =
      fileapi::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypePicasa,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return "";

  picasa_fsids_.insert(fsid);

  if (picasa_fsids_.size() == 1) {
    MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
        FROM_HERE,
        Bind(&ImportedMediaGalleryRegistry::RegisterPicasaFileSystem,
             base::Unretained(this), database_path));
#ifndef NDEBUG
    picasa_database_path_ = database_path;
  } else {
    DCHECK_EQ(picasa_database_path_.value(), database_path.value());
#endif
  }

  return fsid;
}

std::string ImportedMediaGalleryRegistry::RegisterITunesFilesystemOnUIThread(
    const base::FilePath& library_xml_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!library_xml_path.empty());

  std::string fsid =
      fileapi::IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypeItunes,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return std::string();

  itunes_fsids_.insert(fsid);

  if (itunes_fsids_.size() == 1) {
    MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
        FROM_HERE,
        Bind(&ImportedMediaGalleryRegistry::RegisterITunesFileSystem,
             base::Unretained(this), library_xml_path));
#ifndef NDEBUG
    itunes_xml_library_path_ = library_xml_path;
  } else {
    DCHECK_EQ(itunes_xml_library_path_.value(), library_xml_path.value());
#endif
  }

  return fsid;
}

bool ImportedMediaGalleryRegistry::RevokeImportedFilesystemOnUIThread(
    const std::string& fsid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (picasa_fsids_.erase(fsid)) {
    if (picasa_fsids_.empty()) {
      MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokePicasaFileSystem,
               base::Unretained(this)));
    }
    return fileapi::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  }

  if (itunes_fsids_.erase(fsid)) {
    if (itunes_fsids_.empty()) {
      MediaFileSystemMountPointProvider::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokeITunesFileSystem,
               base::Unretained(this)));
    }
    return fileapi::IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  }

  return false;
}

// static
picasa::PicasaDataProvider*
ImportedMediaGalleryRegistry::PicasaDataProvider() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->picasa_data_provider_);
  return GetInstance()->picasa_data_provider_.get();
}

// static
itunes::ITunesDataProvider*
ImportedMediaGalleryRegistry::ITunesDataProvider() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->itunes_data_provider_);
  return GetInstance()->itunes_data_provider_.get();
}

ImportedMediaGalleryRegistry::ImportedMediaGalleryRegistry() {}

ImportedMediaGalleryRegistry::~ImportedMediaGalleryRegistry() {
  DCHECK_EQ(0U, picasa_fsids_.size());
  DCHECK_EQ(0U, itunes_fsids_.size());
}

void ImportedMediaGalleryRegistry::RegisterPicasaFileSystem(
    const base::FilePath& database_path) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!picasa_data_provider_);
  picasa_data_provider_.reset(new picasa::PicasaDataProvider(database_path));
}

void ImportedMediaGalleryRegistry::RevokePicasaFileSystem() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(picasa_data_provider_);
  picasa_data_provider_.reset();
}

void ImportedMediaGalleryRegistry::RegisterITunesFileSystem(
    const base::FilePath& xml_library_path) {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!itunes_data_provider_);
  itunes_data_provider_.reset(new itunes::ITunesDataProvider(xml_library_path));
}

void ImportedMediaGalleryRegistry::RevokeITunesFileSystem() {
  DCHECK(MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(itunes_data_provider_);
  itunes_data_provider_.reset();
}

}  // namespace chrome
