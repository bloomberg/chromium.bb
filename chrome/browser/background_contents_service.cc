// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_contents_service.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/background_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

// Keys for the information we store about individual BackgroundContents in
// prefs. There is one top-level DictionaryValue (stored at
// prefs::kRegisteredBackgroundContents). Information about each
// BackgroundContents is stored under that top-level DictionaryValue, keyed
// by the parent application ID for easy lookup.
//
// kRegisteredBackgroundContents:
//    DictionaryValue {
//       <appid_1>: { "url": <url1>, "name": <frame_name> },
//       <appid_2>: { "url": <url2>, "name": <frame_name> },
//         ... etc ...
//    }
const wchar_t kUrlKey[] = L"url";
const wchar_t kFrameNameKey[] = L"name";

BackgroundContentsService::BackgroundContentsService(
    Profile* profile, const CommandLine* command_line)
    : prefs_(NULL),
      always_keep_alive_(command_line->HasSwitch(switches::kKeepAliveForTest)) {
  // Don't load/store preferences if the proper switch is not enabled, or if
  // the parent profile is off the record.
  if (!profile->IsOffTheRecord() &&
      command_line->HasSwitch(switches::kRestoreBackgroundContents))
    prefs_ = profile->GetPrefs();

  // Listen for events to tell us when to load/unload persisted background
  // contents.
  StartObserving(profile);
}

BackgroundContentsService::~BackgroundContentsService() {
  // BackgroundContents should be shutdown before we go away, as otherwise
  // our browser process refcount will be off.
  DCHECK(contents_map_.empty());
}

void BackgroundContentsService::StartObserving(Profile* profile) {
  // On startup, load our background pages after extension-apps have loaded.
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));

  // Track the lifecycle of all BackgroundContents in the system to allow us
  // to store an up-to-date list of the urls. Start tracking contents when they
  // have been opened, and stop tracking them when they are closed by script.
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_OPENED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_CLOSED,
                 Source<Profile>(profile));

  // Stop tracking BackgroundContents when they have been deleted (happens
  // during shutdown or if the render process dies).
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_DELETED,
                 Source<Profile>(profile));
  // Track when the BackgroundContents navigates to a new URL so we can update
  // our persisted information as appropriate.
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_NAVIGATED,
                 Source<Profile>(profile));

  // Listen for extensions to be unloaded so we can shutdown associated
  // BackgroundContents.
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));

  if (always_keep_alive_ && !profile->IsOffTheRecord()) {
    // For testing, keep the browser process alive until there is an explicit
    // shutdown.
    registrar_.Add(this, NotificationType::APP_TERMINATING,
                   NotificationService::AllSources());
    BrowserList::StartKeepAlive();
  }
}

void BackgroundContentsService::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      LoadBackgroundContentsFromPrefs(Source<Profile>(source).ptr());
      break;
    case NotificationType::BACKGROUND_CONTENTS_DELETED:
      BackgroundContentsShutdown(Details<BackgroundContents>(details).ptr());
      break;
    case NotificationType::BACKGROUND_CONTENTS_OPENED:
      BackgroundContentsOpened(
          Details<BackgroundContentsOpenedDetails>(details).ptr());
      break;
    case NotificationType::BACKGROUND_CONTENTS_CLOSED:
      DCHECK(IsTracked(Details<BackgroundContents>(details).ptr()));
      UnregisterBackgroundContents(Details<BackgroundContents>(details).ptr());
      break;
    case NotificationType::BACKGROUND_CONTENTS_NAVIGATED:
      DCHECK(IsTracked(Details<BackgroundContents>(details).ptr()));
      RegisterBackgroundContents(Details<BackgroundContents>(details).ptr());
      break;
    case NotificationType::EXTENSION_UNLOADED:
      ShutdownAssociatedBackgroundContents(
          ASCIIToUTF16(Details<Extension>(details)->id()));
      break;
    case NotificationType::APP_TERMINATING:
      // Performing an explicit shutdown, so allow the browser process to exit.
      DCHECK(always_keep_alive_);
      BrowserList::EndKeepAlive();
      break;
    default:
      NOTREACHED();
      break;
  }
}

// Loads all background contents whose urls have been stored in prefs.
void BackgroundContentsService::LoadBackgroundContentsFromPrefs(
    Profile* profile) {
  if (!prefs_)
    return;
  const DictionaryValue* contents =
      prefs_->GetDictionary(prefs::kRegisteredBackgroundContents);
  if (!contents)
    return;
  for (DictionaryValue::key_iterator it = contents->begin_keys();
       it != contents->end_keys(); ++it) {
    DictionaryValue* dict;
    contents->GetDictionaryWithoutPathExpansion(*it, &dict);
    string16 frame_name;
    std::string url;
    dict->GetString(kUrlKey, &url);
    dict->GetStringAsUTF16(kFrameNameKey, &frame_name);
    CreateBackgroundContents(profile,
                             GURL(url),
                             frame_name,
                             WideToUTF16(*it));
  }
}

void BackgroundContentsService::CreateBackgroundContents(
    Profile* profile,
    const GURL& url,
    const string16& frame_name,
    const string16& application_id) {
  // We are depending on the fact that we will initialize before any user
  // actions or session restore can take place, so no BackgroundContents should
  // be running yet for the passed application_id.
  DCHECK(!GetAppBackgroundContents(application_id));
  DCHECK(!application_id.empty());
  DCHECK(url.is_valid());
  DLOG(INFO) << "Loading background content url: " << url;

  // Check to make sure that the parent extension is still enabled.
  ExtensionsService* extensions_service = profile->GetExtensionsService();
  Extension* extension = extensions_service->GetExtensionById(
      UTF16ToASCII(application_id), false);

  if (!extension) {
    // We should never reach here - it should not be possible for an application
    // to become uninstalled without the associated BackgroundContents being
    // unregistered via the EXTENSIONS_UNLOADED notification, unless there's a
    // crash before we could save our prefs.
    NOTREACHED() << "No extension found for BackgroundContents - id = " <<
        application_id;
    return;
  }

  BackgroundContents* contents = new BackgroundContents(
      SiteInstance::CreateSiteInstanceForURL(profile, url), MSG_ROUTING_NONE);

  // TODO(atwilson): Change this to send a BACKGROUND_CONTENTS_CREATED
  // notification when we have a listener outside of BackgroundContentsService.
  BackgroundContentsOpenedDetails details = {contents,
                                             frame_name,
                                             application_id};
  BackgroundContentsOpened(&details);

  RenderViewHost* render_view_host = contents->render_view_host();
  // TODO(atwilson): Create RenderViews asynchronously to avoid increasing
  // startup latency (http://crbug.com/47236).
  render_view_host->CreateRenderView(profile->GetRequestContext(), frame_name);
  render_view_host->NavigateToURL(url);
}

void BackgroundContentsService::RegisterBackgroundContents(
    BackgroundContents* background_contents) {
  DCHECK(IsTracked(background_contents));
  if (!prefs_)
    return;

  // We store the first URL we receive for a given application. If there's
  // already an entry for this application, no need to do anything.
  // TODO(atwilson): Verify that this is the desired behavior based on developer
  // feedback (http://crbug.com/47118).
  DictionaryValue* pref = prefs_->GetMutableDictionary(
      prefs::kRegisteredBackgroundContents);
  const string16& appid = GetParentApplicationId(background_contents);
  DictionaryValue* current;
  if (pref->GetDictionaryWithoutPathExpansion(UTF16ToWide(appid), &current))
    return;

  // No entry for this application yet, so add one.
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kUrlKey, background_contents->GetURL().spec());
  dict->SetStringFromUTF16(kFrameNameKey, contents_map_[appid].frame_name);
  pref->SetWithoutPathExpansion(UTF16ToWide(appid), dict);
  prefs_->ScheduleSavePersistentPrefs();
}

void BackgroundContentsService::UnregisterBackgroundContents(
    BackgroundContents* background_contents) {
  if (!prefs_)
    return;
  DCHECK(IsTracked(background_contents));
  const string16 appid = GetParentApplicationId(background_contents);
  DictionaryValue* pref = prefs_->GetMutableDictionary(
      prefs::kRegisteredBackgroundContents);
  pref->RemoveWithoutPathExpansion(UTF16ToWide(appid), NULL);
  prefs_->ScheduleSavePersistentPrefs();
}

void BackgroundContentsService::ShutdownAssociatedBackgroundContents(
    const string16& appid) {
  BackgroundContents* contents = GetAppBackgroundContents(appid);
  if (contents) {
    UnregisterBackgroundContents(contents);
    // Background contents destructor shuts down the renderer.
    delete contents;
  }
}

void BackgroundContentsService::BackgroundContentsOpened(
    BackgroundContentsOpenedDetails* details) {
  // If this is the first BackgroundContents loaded, kick ourselves into
  // persistent mode.
  if (contents_map_.empty())
    BrowserList::StartKeepAlive();

  // Add the passed object to our list. Should not already be tracked.
  DCHECK(!IsTracked(details->contents));
  DCHECK(!details->application_id.empty());
  contents_map_[details->application_id].contents = details->contents;
  contents_map_[details->application_id].frame_name = details->frame_name;
}

// Used by test code and debug checks to verify whether a given
// BackgroundContents is being tracked by this instance.
bool BackgroundContentsService::IsTracked(
    BackgroundContents* background_contents) const {
  return !GetParentApplicationId(background_contents).empty();
}

void BackgroundContentsService::BackgroundContentsShutdown(
    BackgroundContents* background_contents) {
  // Remove the passed object from our list.
  DCHECK(IsTracked(background_contents));
  string16 appid = GetParentApplicationId(background_contents);
  contents_map_.erase(appid);
  // If we have no more BackgroundContents active, then stop keeping the browser
  // process alive.
  if (contents_map_.empty())
    BrowserList::EndKeepAlive();
}

BackgroundContents* BackgroundContentsService::GetAppBackgroundContents(
    const string16& application_id) {
  BackgroundContentsMap::const_iterator it = contents_map_.find(application_id);
  return (it != contents_map_.end()) ? it->second.contents : NULL;
}

const string16& BackgroundContentsService::GetParentApplicationId(
    BackgroundContents* contents) const {
  for (BackgroundContentsMap::const_iterator it = contents_map_.begin();
       it != contents_map_.end(); ++it) {
    if (contents == it->second.contents)
      return it->first;
  }
  return EmptyString16();
}

// static
void BackgroundContentsService::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kRegisteredBackgroundContents);
}
