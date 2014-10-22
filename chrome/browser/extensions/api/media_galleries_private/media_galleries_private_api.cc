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
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/media_galleries/media_file_system_registry.h"
#include "chrome/browser/media_galleries/media_galleries_preferences.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_system.h"

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
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  GalleryWatchManager::OnProfileShutdown(profile_id);
}

// Returns true and sets |gallery_file_path| and |gallery_pref_id| if the
// |gallery_id| is valid and returns false otherwise.
bool GetGalleryFilePathAndId(const std::string& gallery_id,
                             Profile* profile,
                             const Extension* extension,
                             base::FilePath* gallery_file_path,
                             MediaGalleryPrefId* gallery_pref_id) {
  MediaGalleryPrefId pref_id;
  if (!base::StringToUint64(gallery_id, &pref_id))
    return false;
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile);
  base::FilePath file_path(
      preferences->LookUpGalleryPathForExtension(pref_id, extension, false));
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

MediaGalleriesPrivateAPI::MediaGalleriesPrivateAPI(
    content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)), weak_ptr_factory_(this) {
  DCHECK(profile_);
  EventRouter* event_router = EventRouter::Get(profile_);
  event_router->RegisterObserver(
      this, media_galleries_private::OnGalleryChanged::kEventName);
}

MediaGalleriesPrivateAPI::~MediaGalleriesPrivateAPI() {
}

void MediaGalleriesPrivateAPI::Shutdown() {
  EventRouter::Get(profile_)->UnregisterObserver(this);
  weak_ptr_factory_.InvalidateWeakPtrs();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&HandleProfileShutdownOnFileThread, profile_));
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<MediaGalleriesPrivateAPI> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<MediaGalleriesPrivateAPI>*
MediaGalleriesPrivateAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
MediaGalleriesPrivateAPI* MediaGalleriesPrivateAPI::Get(
    content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<MediaGalleriesPrivateAPI>::Get(context);
}

void MediaGalleriesPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  // Make sure MediaGalleriesPreferences is initialized. After that,
  // try to initialize the event router for the listener.
  // This method is called synchronously with the message handler for the
  // JS invocation.

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(profile_);
  preferences->EnsureInitialized(base::Bind(
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
  DCHECK(g_browser_process->media_file_system_registry()->
             GetPreferences(profile_)->IsInitialized());
  tracker_.reset(
      new GalleryWatchStateTracker(profile_));
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateAddGalleryWatchFunction                 //
///////////////////////////////////////////////////////////////////////////////
MediaGalleriesPrivateAddGalleryWatchFunction::
~MediaGalleriesPrivateAddGalleryWatchFunction() {
}

bool MediaGalleriesPrivateAddGalleryWatchFunction::RunAsync() {
  DCHECK(GetProfile());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  scoped_ptr<AddGalleryWatch::Params> params(
      AddGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateAddGalleryWatchFunction::OnPreferencesInit,
      this,
      params->gallery_id));

  return true;
}

void MediaGalleriesPrivateAddGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(pref_id,
                               GetProfile(),
                               extension(),
                               &gallery_file_path,
                               &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    HandleResponse(gallery_pref_id, false);
    return;
  }

  MediaGalleriesPrivateEventRouter* router =
      MediaGalleriesPrivateAPI::Get(GetProfile())->GetEventRouter();

  // TODO(tommycli): The new GalleryWatchManager no longer checks that there is
  // an event listener attached. There should be a check for that here.

  DCHECK(router);
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GalleryWatchManager::SetupGalleryWatch,
                 GetProfile(),
                 gallery_pref_id,
                 gallery_file_path,
                 extension_id(),
                 router->AsWeakPtr()),
      base::Bind(&MediaGalleriesPrivateAddGalleryWatchFunction::HandleResponse,
                 this,
                 gallery_pref_id));
}

void MediaGalleriesPrivateAddGalleryWatchFunction::HandleResponse(
    MediaGalleryPrefId gallery_id,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  media_galleries_private::AddGalleryWatchResult result;
  result.gallery_id = base::Uint64ToString(gallery_id);
  result.success = success;
  SetResult(result.ToValue().release());
  if (success) {
    DCHECK(g_browser_process->media_file_system_registry()
               ->GetPreferences(GetProfile())
               ->IsInitialized());
    GalleryWatchStateTracker* state_tracker = MediaGalleriesPrivateAPI::Get(
        GetProfile())->GetGalleryWatchStateTracker();
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

bool MediaGalleriesPrivateRemoveGalleryWatchFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  scoped_ptr<RemoveGalleryWatch::Params> params(
      RemoveGalleryWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateRemoveGalleryWatchFunction::OnPreferencesInit,
      this,
      params->gallery_id));
  return true;
}

void MediaGalleriesPrivateRemoveGalleryWatchFunction::OnPreferencesInit(
    const std::string& pref_id) {
  base::FilePath gallery_file_path;
  MediaGalleryPrefId gallery_pref_id = 0;
  if (!GetGalleryFilePathAndId(pref_id,
                               GetProfile(),
                               extension(),
                               &gallery_file_path,
                               &gallery_pref_id)) {
    error_ = kInvalidGalleryIDError;
    SendResponse(false);
    return;
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&GalleryWatchManager::RemoveGalleryWatch,
                 GetProfile(),
                 gallery_file_path,
                 extension_id()));

  GalleryWatchStateTracker* state_tracker = MediaGalleriesPrivateAPI::Get(
      GetProfile())->GetGalleryWatchStateTracker();
  state_tracker->OnGalleryWatchRemoved(extension_id(), gallery_pref_id);
  SendResponse(true);
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateGetAllGalleryWatchFunction              //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateGetAllGalleryWatchFunction::
~MediaGalleriesPrivateGetAllGalleryWatchFunction() {
}

bool MediaGalleriesPrivateGetAllGalleryWatchFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateGetAllGalleryWatchFunction::OnPreferencesInit,
      this));
  return true;
}

void MediaGalleriesPrivateGetAllGalleryWatchFunction::OnPreferencesInit() {
  std::vector<std::string> result;
  GalleryWatchStateTracker* state_tracker = MediaGalleriesPrivateAPI::Get(
      GetProfile())->GetGalleryWatchStateTracker();
  MediaGalleryPrefIdSet gallery_ids =
      state_tracker->GetAllWatchedGalleryIDsForExtension(extension_id());
  for (MediaGalleryPrefIdSet::const_iterator iter = gallery_ids.begin();
       iter != gallery_ids.end(); ++iter) {
    result.push_back(base::Uint64ToString(*iter));
  }
  results_ = GetAllGalleryWatch::Results::Create(result);
  SendResponse(true);
}

///////////////////////////////////////////////////////////////////////////////
//              MediaGalleriesPrivateRemoveAllGalleryWatchFunction           //
///////////////////////////////////////////////////////////////////////////////

MediaGalleriesPrivateRemoveAllGalleryWatchFunction::
~MediaGalleriesPrivateRemoveAllGalleryWatchFunction() {
}

bool MediaGalleriesPrivateRemoveAllGalleryWatchFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!render_view_host() || !render_view_host()->GetProcess())
    return false;

  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  preferences->EnsureInitialized(base::Bind(
      &MediaGalleriesPrivateRemoveAllGalleryWatchFunction::OnPreferencesInit,
      this));
  return true;
}

void MediaGalleriesPrivateRemoveAllGalleryWatchFunction::OnPreferencesInit() {
  MediaGalleriesPreferences* preferences =
      g_browser_process->media_file_system_registry()->GetPreferences(
          GetProfile());
  GalleryWatchStateTracker* state_tracker = MediaGalleriesPrivateAPI::Get(
      GetProfile())->GetGalleryWatchStateTracker();
  state_tracker->RemoveAllGalleryWatchersForExtension(
      extension_id(), preferences);
  SendResponse(true);
}

}  // namespace extensions
