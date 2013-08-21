// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_api.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/media_galleries_private/gallery_watch_manager.h"
#include "chrome/browser/extensions/api/media_galleries_private/media_galleries_private_event_router.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/common/extensions/api/media_galleries_private/media_galleries_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"

using base::DictionaryValue;
using base::ListValue;

namespace extensions {

namespace AddGalleryWatch =
    extensions::api::media_galleries_private::AddGalleryWatch;
namespace RemoveGalleryWatch =
    extensions::api::media_galleries_private::RemoveGalleryWatch;
namespace GetAllGalleryWatch =
    extensions::api::media_galleries_private::GetAllGalleryWatch;
namespace media_galleries_private =
    api::media_galleries_private;

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
bool GetGalleryFilePathAndId(const std::string& gallery_id,
                             Profile* profile,
                             const Extension* extension,
                             base::FilePath* gallery_file_path,
                             chrome::MediaGalleryPrefId* gallery_pref_id) {
  chrome::MediaGalleryPrefId pref_id;
  if (!base::StringToUint64(gallery_id, &pref_id))
    return false;
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
      weak_ptr_factory_(this) {
  DCHECK(profile_);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, media_galleries_private::OnDeviceAttached::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, media_galleries_private::OnDeviceDetached::kEventName);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, media_galleries_private::OnGalleryChanged::kEventName);
}

MediaGalleriesPrivateAPI::~MediaGalleriesPrivateAPI() {
}

void MediaGalleriesPrivateAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  // Start the StorageMonitor if it is not already initialized. After that,
  // try to initialize the event router for the listener.
  // This method is called synchronously with the message handler for the
  // JS invocation.

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateAPI::MaybeInitializeEventRouterAndTracker,
      weak_ptr_factory_.GetWeakPtr()));
}

MediaGalleriesPrivateEventRouter* MediaGalleriesPrivateAPI::GetEventRouter() {
  MaybeInitializeEventRouterAndTracker();
  return media_galleries_private_event_router_.get();
}

GalleryWatchStateTracker*
MediaGalleriesPrivateAPI::GetGalleryWatchStateTracker() {
  MaybeInitializeEventRouterAndTracker();
  return tracker_.get();
}

void MediaGalleriesPrivateAPI::MaybeInitializeEventRouterAndTracker() {
  if (media_galleries_private_event_router_.get())
    return;
  media_galleries_private_event_router_.reset(
      new MediaGalleriesPrivateEventRouter(profile_));
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
  tracker_.reset(
      new GalleryWatchStateTracker(profile_));
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

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateAddGalleryWatchFunction::OnStorageMonitorInit,
      this,
      params->gallery_id));

  return true;
}

void MediaGalleriesPrivateAddGalleryWatchFunction::OnStorageMonitorInit(
    const std::string& pref_id) {
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
  base::FilePath gallery_file_path;
  chrome::MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(pref_id, profile_, GetExtension(),
                               &gallery_file_path, &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    HandleResponse(gallery_pref_id, false);
    return;
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
}

void MediaGalleriesPrivateAddGalleryWatchFunction::HandleResponse(
    chrome::MediaGalleryPrefId gallery_id,
    bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  media_galleries_private::AddGalleryWatchResult result;
  result.gallery_id = base::Uint64ToString(gallery_id);
  result.success = success;
  SetResult(result.ToValue().release());
  if (success) {
    DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
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
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  // Remove gallery watch operation is currently supported on windows platforms.
  // Please refer to crbug.com/144491 for more details.
  scoped_ptr<RemoveGalleryWatch::Params> params(
      RemoveGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateRemoveGalleryWatchFunction::OnStorageMonitorInit,
      this,
      params->gallery_id));
  return true;
}

void MediaGalleriesPrivateRemoveGalleryWatchFunction::OnStorageMonitorInit(
    const std::string& pref_id) {
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
#if defined(OS_WIN)
  base::FilePath gallery_file_path;
  chrome::MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(pref_id, profile_, GetExtension(),
                               &gallery_file_path, &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    SendResponse(false);
    return;
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
  SendResponse(true);
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

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateGetAllGalleryWatchFunction::OnStorageMonitorInit,
      this));
  return true;
}

void MediaGalleriesPrivateGetAllGalleryWatchFunction::OnStorageMonitorInit() {
  std::vector<std::string> result;
#if defined(OS_WIN)
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
  GalleryWatchStateTracker* state_tracker =
      MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
  chrome::MediaGalleryPrefIdSet gallery_ids =
      state_tracker->GetAllWatchedGalleryIDsForExtension(extension_id());
  for (chrome::MediaGalleryPrefIdSet::const_iterator iter =
           gallery_ids.begin();
       iter != gallery_ids.end(); ++iter) {
    result.push_back(base::Uint64ToString(*iter));
  }
#endif
  results_ = GetAllGalleryWatch::Results::Create(result);
  SendResponse(true);
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateRemoveAllGalleryWatchFunction           //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateRemoveAllGalleryWatchFunction::
~MediaGalleriesPrivateRemoveAllGalleryWatchFunction() {
}

bool MediaGalleriesPrivateRemoveAllGalleryWatchFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  chrome::StorageMonitor::GetInstance()->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateRemoveAllGalleryWatchFunction::OnStorageMonitorInit,
      this));
  return true;
}

void
MediaGalleriesPrivateRemoveAllGalleryWatchFunction::OnStorageMonitorInit() {
#if defined(OS_WIN)
  DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
  chrome::MediaFileSystemRegistry* registry =
      g_browser_process->media_file_system_registry();
  chrome::MediaGalleriesPreferences* preferences =
      registry->GetPreferences(profile_);
  GalleryWatchStateTracker* state_tracker =
      MediaGalleriesPrivateAPI::Get(profile_)->GetGalleryWatchStateTracker();
  state_tracker->RemoveAllGalleryWatchersForExtension(
      extension_id(), preferences);
#endif
  SendResponse(true);
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateGetHandlersFunction                     //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateGetHandlersFunction::
~MediaGalleriesPrivateGetHandlersFunction() {
}

bool MediaGalleriesPrivateGetHandlersFunction::RunImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);

  ListValue* result_list = new ListValue;

  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end();
       ++iter) {
    const Extension* extension = iter->get();
    if (profile_->IsOffTheRecord() &&
        !service->IsIncognitoEnabled(extension->id()))
      continue;

    MediaGalleriesHandler::List* handler_list =
        MediaGalleriesHandler::GetHandlers(extension);
    if (!handler_list)
      continue;

    for (MediaGalleriesHandler::List::const_iterator action_iter =
             handler_list->begin();
         action_iter != handler_list->end();
         ++action_iter) {
      const MediaGalleriesHandler* action = action_iter->get();
      DictionaryValue* handler = new DictionaryValue;
      handler->SetString("extensionId", action->extension_id());
      handler->SetString("id", action->id());
      handler->SetString("title", action->title());
      handler->SetString("iconUrl", action->icon_path());
      result_list->Append(handler);
    }
  }

  SetResult(result_list);
  SendResponse(true);

  return true;
}

}  // namespace extensions
