// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/imported_media_gallery_registry.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/media_galleries/fileapi/iphoto_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/itunes_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/browser/media_galleries/fileapi/picasa_data_provider.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/fileapi/isolated_context.h"

using base::Bind;
using fileapi::IsolatedContext;

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

  std::string fsid;

#if defined(OS_WIN) || defined(OS_MACOSX)
  fsid = IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypePicasa,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return fsid;

  picasa_fsids_.insert(fsid);

  if (picasa_fsids_.size() == 1) {
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        Bind(&ImportedMediaGalleryRegistry::RegisterPicasaFileSystem,
             base::Unretained(this), database_path));
#ifndef NDEBUG
    picasa_database_path_ = database_path;
  } else {
    DCHECK_EQ(picasa_database_path_.value(), database_path.value());
#endif
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

  return fsid;
}

std::string ImportedMediaGalleryRegistry::RegisterITunesFilesystemOnUIThread(
    const base::FilePath& library_xml_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!library_xml_path.empty());

  std::string fsid;

#if defined(OS_WIN) || defined(OS_MACOSX)
  fsid = IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypeItunes,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return fsid;

  itunes_fsids_.insert(fsid);

  if (itunes_fsids_.size() == 1) {
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

  return fsid;
}

std::string ImportedMediaGalleryRegistry::RegisterIPhotoFilesystemOnUIThread(
    const base::FilePath& library_xml_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!library_xml_path.empty());

  std::string fsid;

  // TODO(gbillock): Investigate how to refactor this to reduce duplicated
  // code.
#if defined(OS_MACOSX)
  fsid = IsolatedContext::GetInstance()->RegisterFileSystemForVirtualPath(
           fileapi::kFileSystemTypeIphoto,
           extension_misc::kMediaFileSystemPathPart,
           base::FilePath());

  if (fsid.empty())
    return fsid;

  iphoto_fsids_.insert(fsid);

  if (iphoto_fsids_.size() == 1) {
    MediaFileSystemBackend::MediaTaskRunner()->PostTask(
        FROM_HERE,
        Bind(&ImportedMediaGalleryRegistry::RegisterIPhotoFileSystem,
             base::Unretained(this), library_xml_path));
#ifndef NDEBUG
    iphoto_xml_library_path_ = library_xml_path;
  } else {
    DCHECK_EQ(iphoto_xml_library_path_.value(), library_xml_path.value());
#endif
  }
#endif  // defined(OS_MACOSX)

  return fsid;
}

bool ImportedMediaGalleryRegistry::RevokeImportedFilesystemOnUIThread(
    const std::string& fsid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if defined(OS_WIN) || defined(OS_MACOSX)
  if (picasa_fsids_.erase(fsid)) {
    if (picasa_fsids_.empty()) {
      MediaFileSystemBackend::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokePicasaFileSystem,
               base::Unretained(this)));
    }
    return IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  }

  if (itunes_fsids_.erase(fsid)) {
    if (itunes_fsids_.empty()) {
      MediaFileSystemBackend::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokeITunesFileSystem,
               base::Unretained(this)));
    }
    return IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  }
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_MACOSX)
  if (iphoto_fsids_.erase(fsid)) {
    if (iphoto_fsids_.empty()) {
      MediaFileSystemBackend::MediaTaskRunner()->PostTask(
          FROM_HERE,
          Bind(&ImportedMediaGalleryRegistry::RevokeIPhotoFileSystem,
               base::Unretained(this)));
    }
    return IsolatedContext::GetInstance()->RevokeFileSystem(fsid);
  }
#endif  // defined(OS_MACOSX)

  return false;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// static
picasa::PicasaDataProvider*
ImportedMediaGalleryRegistry::PicasaDataProvider() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->picasa_data_provider_);
  return GetInstance()->picasa_data_provider_.get();
}

// static
itunes::ITunesDataProvider*
ImportedMediaGalleryRegistry::ITunesDataProvider() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->itunes_data_provider_);
  return GetInstance()->itunes_data_provider_.get();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_MACOSX)
// static
iphoto::IPhotoDataProvider*
ImportedMediaGalleryRegistry::IPhotoDataProvider() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(GetInstance()->iphoto_data_provider_);
  return GetInstance()->iphoto_data_provider_.get();
}
#endif  // defined(OS_MACOSX)

ImportedMediaGalleryRegistry::ImportedMediaGalleryRegistry() {}

ImportedMediaGalleryRegistry::~ImportedMediaGalleryRegistry() {
#if defined(OS_WIN) || defined(OS_MACOSX)
  DCHECK_EQ(0U, picasa_fsids_.size());
  DCHECK_EQ(0U, itunes_fsids_.size());
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
#if defined(OS_MACOSX)
  DCHECK_EQ(0U, iphoto_fsids_.size());
#endif  // defined(OS_MACOSX)
}

#if defined(OS_WIN) || defined(OS_MACOSX)
void ImportedMediaGalleryRegistry::RegisterPicasaFileSystem(
    const base::FilePath& database_path) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!picasa_data_provider_);
  picasa_data_provider_.reset(new picasa::PicasaDataProvider(database_path));
}

void ImportedMediaGalleryRegistry::RevokePicasaFileSystem() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(picasa_data_provider_);
  picasa_data_provider_.reset();
}

void ImportedMediaGalleryRegistry::RegisterITunesFileSystem(
    const base::FilePath& xml_library_path) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!itunes_data_provider_);
  itunes_data_provider_.reset(new itunes::ITunesDataProvider(xml_library_path));
}

void ImportedMediaGalleryRegistry::RevokeITunesFileSystem() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(itunes_data_provider_);
  itunes_data_provider_.reset();
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_MACOSX)
void ImportedMediaGalleryRegistry::RegisterIPhotoFileSystem(
    const base::FilePath& xml_library_path) {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(!iphoto_data_provider_);
  iphoto_data_provider_.reset(new iphoto::IPhotoDataProvider(xml_library_path));
}

void ImportedMediaGalleryRegistry::RevokeIPhotoFileSystem() {
  DCHECK(MediaFileSystemBackend::CurrentlyOnMediaTaskRunnerThread());
  DCHECK(iphoto_data_provider_);
  iphoto_data_provider_.reset();
}
#endif  // defined(OS_MACOSX)
