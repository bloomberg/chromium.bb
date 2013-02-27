// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/media_gallery/media_galleries_preferences.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"

namespace extensions {

namespace AddGalleryWatch =
    extensions::api::media_galleries_private::AddGalleryWatch;
namespace RemoveGalleryWatch =
    extensions::api::media_galleries_private::RemoveGalleryWatch;
namespace GetAllGalleryWatch =
    extensions::api::media_galleries_private::GetAllGalleryWatch;
namespace EjectDevice =
    extensions::api::media_galleries_private::EjectDevice;

namespace {

const char kInvalidGalleryIDError[] = "Invalid gallery ID";

// Handles the profile shutdown event on the file thread to clean up
// GalleryWatchManager.
void HandleProfileShutdownOnFileThread(void* profile_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  GalleryWatchManager::OnProfileShutdown(profile_id);
}

// Gets the |gallery_file_path| and |gallery_pref_id| of the gallery specified
// by the |gallery_id|. Returns true and set |gallery_file_path| and
// |gallery_pref_id| if the |gallery_id| is valid and returns false otherwise.
bool GetGalleryFilePathAndId(int gallery_id,
                             Profile* profile,
                             const Extension* extension,
                             base::FilePath* gallery_file_path,
                             chrome::MediaGalleryPrefId* gallery_pref_id) {
  if (gallery_id < 0)
    return false;
  chrome::MediaGalleryPrefId pref_id = static_cast<uint64>(gallery_id);
  chrome::MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
   base::FilePath file_path(
      registry->GetPreferences(profile)->LookUpGalleryPathForExtension(
          pref_id, extension, false));
  if (file_path.empty())
    return false;
  *gallery_pref_id = pref_id;
  *gallery_file_path = file_path;
  return true;
}

}  // namespace


///////////////////////////////////////////////////////////////////////////////
//                      MediaGalleriesPrivateAPI                             //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateAPI::MediaGalleriesPrivateAPI(Profile* profile)
    : profile_(profile),
      tracker_(profile) {
  DCHECK(profile_);
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

static base::LazyInstance<ProfileKeyedAPIFactory<MediaGalleriesPrivateAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<MediaGalleriesPrivateAPI>*
    MediaGalleriesPrivateAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
MediaGalleriesPrivateAPI* MediaGalleriesPrivateAPI::Get(Profile* profile) {
  return
      ProfileKeyedAPIFactory<MediaGalleriesPrivateAPI>::GetForProfile(profile);
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

GalleryWatchStateTracker*
MediaGalleriesPrivateAPI::GetGalleryWatchStateTracker() {
  return &tracker_;
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
  base::FilePath gallery_file_path;
  chrome::MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(params->gallery_id, profile_, GetExtension(),
                               &gallery_file_path, &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

#if defined(OS_WIN)
  MediaGalleriesPrivateEventRouter* router =
      MediaGalleriesPrivateAPI::Get(profile_)->GetEventRouter();
  DCHECK(router);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GalleryWatchManager::SetupGalleryWatch,
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
  if (success) {
    GalleryWatchStateTracker* state_tracker =
        MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
    state_tracker->OnGalleryWatchAdded(extension_id(), gallery_id);
  }
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

  base::FilePath gallery_file_path;
  chrome::MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(params->gallery_id, profile_, GetExtension(),
                               &gallery_file_path, &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    return false;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&GalleryWatchManager::RemoveGalleryWatch,
                 profile_,
                 gallery_file_path,
                 extension_id()));

  GalleryWatchStateTracker* state_tracker =
      MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
  state_tracker->OnGalleryWatchRemoved(extension_id(), gallery_pref_id);
#endif
  return true;
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateGetAllGalleryWatchFunction              //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateGetAllGalleryWatchFunction::
~MediaGalleriesPrivateGetAllGalleryWatchFunction() {
}

bool MediaGalleriesPrivateGetAllGalleryWatchFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  std::vector<int> result;
#if defined(OS_WIN)
  GalleryWatchStateTracker* state_tracker =
      MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
  chrome::MediaGalleryPrefIdSet gallery_ids =
      state_tracker->GetAllWatchedGalleryIDsForExtension(extension_id());
  for (chrome::MediaGalleryPrefIdSet::const_iterator iter =
           gallery_ids.begin();
       iter != gallery_ids.end(); ++iter) {
    if (*iter < kint32max)
      result.push_back(*iter);
  }
#endif
  results_ = GetAllGalleryWatch::Results::Create(result);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateRemoveAllGalleryWatchFunction           //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateRemoveAllGalleryWatchFunction::
~MediaGalleriesPrivateRemoveAllGalleryWatchFunction() {
}

bool MediaGalleriesPrivateRemoveAllGalleryWatchFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_WIN)
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  GalleryWatchStateTracker* state_tracker =
      MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
  state_tracker->RemoveAllGalleryWatchersForExtension(extension_id());
#endif
  return true;
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateEjectDeviceFunction                     //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateEjectDeviceFunction::
~MediaGalleriesPrivateEjectDeviceFunction() {
}

bool MediaGalleriesPrivateEjectDeviceFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<EjectDevice::Params> params(EjectDevice::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  chrome::StorageMonitor* monitor = chrome::StorageMonitor::GetInstance();
  std::string device_id_str =
      monitor->GetDeviceIdForTransientId(params->device_id);
  if (device_id_str == "") {
    HandleResponse(chrome::StorageMonitor::EJECT_NO_SUCH_DEVICE);
    return true;
  }

  monitor->EjectDevice(
      device_id_str,
      base::Bind(&MediaGalleriesPrivateEjectDeviceFunction::HandleResponse,
                 base::Unretained(this)));

  return true;
}

void MediaGalleriesPrivateEjectDeviceFunction::HandleResponse(
    chrome::StorageMonitor::EjectStatus status) {

  using extensions::api::media_galleries_private::
      EJECT_DEVICE_RESULT_CODE_FAILURE;
  using extensions::api::media_galleries_private::
      EJECT_DEVICE_RESULT_CODE_IN_USE;
  using extensions::api::media_galleries_private::
      EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;
  using extensions::api::media_galleries_private::
      EJECT_DEVICE_RESULT_CODE_SUCCESS;
  using extensions::api::media_galleries_private::EjectDeviceResultCode;

  EjectDeviceResultCode result = EJECT_DEVICE_RESULT_CODE_FAILURE;
  if (status == chrome::StorageMonitor::EJECT_OK)
    result = EJECT_DEVICE_RESULT_CODE_SUCCESS;
  if (status == chrome::StorageMonitor::EJECT_IN_USE)
    result = EJECT_DEVICE_RESULT_CODE_IN_USE;
  if (status == chrome::StorageMonitor::EJECT_NO_SUCH_DEVICE)
    result = EJECT_DEVICE_RESULT_CODE_NO_SUCH_DEVICE;

  SetResult(base::StringValue::CreateStringValue(
      api::media_galleries_private::ToString(result)));
  SendResponse(true);
}

}  // namespace extensions
