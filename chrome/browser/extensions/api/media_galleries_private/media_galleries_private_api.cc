// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/location.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api_factory.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_gallery_extension_notification_observer.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "chrome/browser/system_monitor/media_storage_util.h"
#include "chrome/common/extensions/api/media_galleries_private.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"

namespace extensions {

namespace AddGalleryWatch =
    extensions::api::media_galleries_private::AddGalleryWatch;
namespace RemoveGalleryWatch =
    extensions::api::media_galleries_private::RemoveGalleryWatch;

namespace {

const char kInvalidGalleryIDError[] = "Invalid gallery ID";

// Returns the absolute file path of the gallery specified by the |gallery_id|.
// Returns an empty file path if the |gallery_id| is invalid.
FilePath GetGalleryAbsolutePath(chrome::MediaGalleryPrefId gallery_id,
                                Profile* profile,
                                const Extension* extension) {
  DCHECK(profile);
  DCHECK(extension);
  chrome::MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  DCHECK(registry);
  chrome::MediaGalleriesPreferences* preferences =
      registry->GetPreferences(profile);
  if (!preferences)
    return FilePath();

  const chrome::MediaGalleryPrefIdSet permitted_galleries =
      preferences->GalleriesForExtension(*extension);
  if (permitted_galleries.empty() ||
      permitted_galleries.find(gallery_id) == permitted_galleries.end())
    return FilePath();

  const chrome::MediaGalleriesPrefInfoMap& galleries_info =
      preferences->known_galleries();
  return chrome::MediaStorageUtil::FindDevicePathById(
      galleries_info.find(gallery_id)->second.device_id);
}

// Handles the profile shutdown event on the file thread to clean up
// GalleryWatchManager.
void HandleProfileShutdownOnFileThread(void* profile_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  GalleryWatchManager::OnProfileShutdown(profile_id);
}

// Sets up a gallery watch on the file thread for the extension specified by the
// |extension_id|.
// |profile_id| specifies the extension profile identifier.
// |gallery_id| specifies the gallery identifier.
// |gallery_file_path| specifies the absolute gallery path.
// Returns true, if the watch setup operation was successful.
bool SetupGalleryWatchOnFileThread(
    void* profile_id,
    chrome::MediaGalleryPrefId gallery_id,
    const FilePath& gallery_file_path,
    const std::string& extension_id,
    base::WeakPtr<MediaGalleriesPrivateEventRouter> event_router) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  return GalleryWatchManager::GetForProfile(profile_id)->StartGalleryWatch(
      gallery_id, gallery_file_path, extension_id, event_router);
}

// Cancels the gallery watch for the extension specified by the |extension_id|
// on the file thread.
void RemoveGalleryWatchOnFileThread(void* profile_id,
                                    const FilePath& gallery_file_path,
                                    const std::string& extension_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  if (!GalleryWatchManager::HasForProfile(profile_id))
    return;
  GalleryWatchManager::GetForProfile(profile_id)->StopGalleryWatch(
      gallery_file_path, extension_id);
}

}  // namespace


///////////////////////////////////////////////////////////////////////////////
//                      MediaGalleriesPrivateAPI                             //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateAPI::MediaGalleriesPrivateAPI(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  extension_notification_observer_.reset(
      new MediaGalleryExtensionNotificationObserver(profile_));
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnAttachEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnDetachEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnGalleryChangedEventName);
}

MediaGalleriesPrivateAPI::~MediaGalleriesPrivateAPI() {
}

void MediaGalleriesPrivateAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandleProfileShutdownOnFileThread, profile_));
}

void MediaGalleriesPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  // Try to initialize the event router for the listener. If
  // MediaGalleriesPrivateAPI::GetEventRouter() was called before adding
  // the listener, router would be initialized.
  MaybeInitializeEventRouter();
}

MediaGalleriesPrivateEventRouter* MediaGalleriesPrivateAPI::GetEventRouter() {
  MaybeInitializeEventRouter();
  return media_galleries_private_event_router_.get();
}

void MediaGalleriesPrivateAPI::MaybeInitializeEventRouter() {
  if (media_galleries_private_event_router_.get())
    return;
  media_galleries_private_event_router_.reset(
      new MediaGalleriesPrivateEventRouter(profile_));
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateAddGalleryWatchFunction                 //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesPrivateAddGalleryWatchFunction::
~MediaGalleriesPrivateAddGalleryWatchFunction() {
}

bool MediaGalleriesPrivateAddGalleryWatchFunction::RunImpl() {
  DCHECK(profile_);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  scoped_ptr<AddGalleryWatch::Params> params(
      AddGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->gallery_id < 0) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

  // First param is the gallery identifier to watch.
  chrome::MediaGalleryPrefId gallery_pref_id =
      static_cast<uint64>(params->gallery_id);
  FilePath gallery_file_path(
      GetGalleryAbsolutePath(gallery_pref_id, profile_, GetExtension()));
  if (gallery_file_path.empty()) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

#if defined(OS_WIN)
  MediaGalleriesPrivateEventRouter* router =
      MediaGalleriesPrivateAPIFactory::GetForProfile(
          profile_)->GetEventRouter();
  DCHECK(router);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SetupGalleryWatchOnFileThread,
                 profile_,
                 gallery_pref_id,
                 gallery_file_path,
                 extension_id(),
                 router->AsWeakPtr()),
      base::Bind(&MediaGalleriesPrivateAddGalleryWatchFunction::HandleResponse,
                 this,
                 gallery_pref_id));
#else
  // Recursive gallery watch operation is not currently supported on
  // non-windows platforms. Please refer to crbug.com/144491 for more details.
  HandleResponse(gallery_pref_id, false);
#endif
  return true;
}

void MediaGalleriesPrivateAddGalleryWatchFunction::HandleResponse(
    chrome::MediaGalleryPrefId gallery_id,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  extensions::api::media_galleries_private::AddGalleryWatchResult result;
  result.gallery_id = gallery_id;
  result.success = success;
  SetResult(result.ToValue().release());
  SendResponse(true);
}


///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateRemoveGalleryWatchFunction              //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateRemoveGalleryWatchFunction::
~MediaGalleriesPrivateRemoveGalleryWatchFunction() {
}

bool MediaGalleriesPrivateRemoveGalleryWatchFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_WIN)
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // Remove gallery watch operation is currently supported on windows platforms.
  // Please refer to crbug.com/144491 for more details.
  scoped_ptr<RemoveGalleryWatch::Params> params(
      RemoveGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->gallery_id < 0) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

  // First param is the gallery identifier.
  chrome::MediaGalleryPrefId gallery_pref_id =
      static_cast<uint64>(params->gallery_id);

  FilePath gallery_file_path(
      GetGalleryAbsolutePath(gallery_pref_id, profile_, GetExtension()));
  if (gallery_file_path.empty()) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&RemoveGalleryWatchOnFileThread,
                 profile_,
                 gallery_file_path,
                 extension_id()));
#endif
  return true;
}

}  // namespace extensions
