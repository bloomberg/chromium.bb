// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_contents_service.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void CloseBalloon(const std::string id) {
  g_browser_process->notification_ui_manager()->CancelById(id);
}

class CrashNotificationDelegate : public NotificationDelegate {
 public:
  CrashNotificationDelegate(Profile* profile, const Extension* extension)
      : profile_(profile),
        is_app_(extension->is_app()),
        extension_id_(extension->id()) {
  }

  ~CrashNotificationDelegate() {
  }

  void Display() {}

  void Error() {}

  void Close(bool by_user) {}

  void Click() {
    if (is_app_) {
      profile_->GetBackgroundContentsService()->
          LoadBackgroundContentsForExtension(profile_, extension_id_);
    } else {
      profile_->GetExtensionService()->ReloadExtension(extension_id_);
    }

    // Closing the balloon here should be OK, but it causes a crash on mac
    // http://crbug.com/78167
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableFunction(&CloseBalloon, id()));
  }

  std::string id() const {
    return "app.background.crashed." + extension_id_;
  }

 private:
  Profile* profile_;
  bool is_app_;
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(CrashNotificationDelegate);
};

void ShowBalloon(const Extension* extension, Profile* profile) {
  string16 message = l10n_util::GetStringFUTF16(
      extension->is_app() ?  IDS_BACKGROUND_CRASHED_APP_BALLOON_MESSAGE :
      IDS_BACKGROUND_CRASHED_EXTENSION_BALLOON_MESSAGE,
      UTF8ToUTF16(extension->name()));
  string16 content_url = DesktopNotificationService::CreateDataUrl(
      extension->GetIconURL(Extension::EXTENSION_ICON_SMALLISH,
                            ExtensionIconSet::MATCH_BIGGER),
      string16(), message, WebKit::WebTextDirectionDefault);
  Notification notification(
      extension->url(), GURL(content_url), string16(), string16(),
      new CrashNotificationDelegate(profile, extension));
  g_browser_process->notification_ui_manager()->Add(notification, profile);
}

}

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
const char kUrlKey[] = "url";
const char kFrameNameKey[] = "name";

BackgroundContentsService::BackgroundContentsService(
    Profile* profile, const CommandLine* command_line)
    : prefs_(NULL) {
  // Don't load/store preferences if the proper switch is not enabled, or if
  // the parent profile is incognito.
  if (!profile->IsOffTheRecord() &&
      !command_line->HasSwitch(switches::kDisableRestoreBackgroundContents))
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

std::vector<BackgroundContents*>
BackgroundContentsService::GetBackgroundContents() const
{
  std::vector<BackgroundContents*> contents;
  for (BackgroundContentsMap::const_iterator it = contents_map_.begin();
       it != contents_map_.end(); ++it)
    contents.push_back(it->second.contents);
  return contents;
}

void BackgroundContentsService::StartObserving(Profile* profile) {
  // On startup, load our background pages after extension-apps have loaded.
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile));

  // Track the lifecycle of all BackgroundContents in the system to allow us
  // to store an up-to-date list of the urls. Start tracking contents when they
  // have been opened via CreateBackgroundContents(), and stop tracking them
  // when they are closed by script.
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

  // Track when the extensions crash so that the user can be notified
  // about it, and the crashed contents can be restarted.
  registrar_.Add(this, NotificationType::EXTENSION_PROCESS_TERMINATED,
                 Source<Profile>(profile));
  registrar_.Add(this, NotificationType::BACKGROUND_CONTENTS_TERMINATED,
                 Source<Profile>(profile));

  // Listen for extensions to be unloaded so we can shutdown associated
  // BackgroundContents.
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile));
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
    case NotificationType::BACKGROUND_CONTENTS_CLOSED:
      DCHECK(IsTracked(Details<BackgroundContents>(details).ptr()));
      UnregisterBackgroundContents(Details<BackgroundContents>(details).ptr());
      break;
    case NotificationType::BACKGROUND_CONTENTS_NAVIGATED:
      DCHECK(IsTracked(Details<BackgroundContents>(details).ptr()));
      RegisterBackgroundContents(Details<BackgroundContents>(details).ptr());
      break;

    case NotificationType::EXTENSION_PROCESS_TERMINATED:
    case NotificationType::BACKGROUND_CONTENTS_TERMINATED: {
      Profile* profile = Source<Profile>(source).ptr();
      const Extension* extension = NULL;
      if (type.value == NotificationType::BACKGROUND_CONTENTS_TERMINATED) {
        BackgroundContents* bg =
            Details<BackgroundContents>(details).ptr();
        std::string extension_id = UTF16ToASCII(profile->
            GetBackgroundContentsService()->GetParentApplicationId(bg));
        extension =
          profile->GetExtensionService()->GetExtensionById(extension_id, false);
      } else {
        ExtensionHost* extension_host = Details<ExtensionHost>(details).ptr();
        extension = extension_host->extension();
      }
      if (!extension)
        break;

      // When an extension crashes, EXTENSION_PROCESS_TERMINATED is followed by
      // an EXTENSION_UNLOADED notification. This UNLOADED signal causes all the
      // notifications for this extension to be cancelled by
      // DesktopNotificationService. For this reason, instead of showing the
      // balloon right now, we schedule it to show a little later.
      MessageLoop::current()->PostTask(FROM_HERE,
          NewRunnableFunction(&ShowBalloon, extension, profile));
      break;
    }
    case NotificationType::EXTENSION_UNLOADED:
      switch (Details<UnloadedExtensionInfo>(details)->reason) {
        case UnloadedExtensionInfo::DISABLE:  // Intentionally fall through.
        case UnloadedExtensionInfo::UNINSTALL:
          ShutdownAssociatedBackgroundContents(
              ASCIIToUTF16(
                  Details<UnloadedExtensionInfo>(details)->extension->id()));
          break;
        case UnloadedExtensionInfo::UPDATE:
          // Leave BackgroundContents in place
          break;
        default:
          NOTREACHED();
          ShutdownAssociatedBackgroundContents(
              ASCIIToUTF16(
                  Details<UnloadedExtensionInfo>(details)->extension->id()));
          break;
      }
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
  ExtensionService* extensions_service = profile->GetExtensionService();
  DCHECK(extensions_service);
  for (DictionaryValue::key_iterator it = contents->begin_keys();
       it != contents->end_keys(); ++it) {
    // Check to make sure that the parent extension is still enabled.
    const Extension* extension = extensions_service->GetExtensionById(
        *it, false);
    if (!extension) {
      // We should never reach here - it should not be possible for an app
      // to become uninstalled without the associated BackgroundContents being
      // unregistered via the EXTENSIONS_UNLOADED notification, unless there's a
      // crash before we could save our prefs.
      NOTREACHED() << "No extension found for BackgroundContents - id = "
                   << *it;
      return;
    }
    LoadBackgroundContentsFromDictionary(profile, *it, contents);
  }
}

void BackgroundContentsService::LoadBackgroundContentsForExtension(
    Profile* profile,
    const std::string& extension_id) {
  if (!prefs_)
    return;
  const DictionaryValue* contents =
      prefs_->GetDictionary(prefs::kRegisteredBackgroundContents);
  if (!contents)
    return;
  LoadBackgroundContentsFromDictionary(profile, extension_id, contents);
}

void BackgroundContentsService::LoadBackgroundContentsFromDictionary(
    Profile* profile,
    const std::string& extension_id,
    const DictionaryValue* contents) {
  ExtensionService* extensions_service = profile->GetExtensionService();
  DCHECK(extensions_service);

  DictionaryValue* dict;
  contents->GetDictionaryWithoutPathExpansion(extension_id, &dict);
  if (dict == NULL)
    return;
  string16 frame_name;
  std::string url;
  dict->GetString(kUrlKey, &url);
  dict->GetString(kFrameNameKey, &frame_name);
  LoadBackgroundContents(profile,
                         GURL(url),
                         frame_name,
                         UTF8ToUTF16(extension_id));
}

void BackgroundContentsService::LoadBackgroundContents(
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
  DVLOG(1) << "Loading background content url: " << url;

  BackgroundContents* contents = CreateBackgroundContents(
      SiteInstance::CreateSiteInstanceForURL(profile, url),
      MSG_ROUTING_NONE,
      profile,
      frame_name,
      application_id);

  RenderViewHost* render_view_host = contents->render_view_host();
  // TODO(atwilson): Create RenderViews asynchronously to avoid increasing
  // startup latency (http://crbug.com/47236).
  render_view_host->CreateRenderView(frame_name);
  render_view_host->NavigateToURL(url);
}

BackgroundContents* BackgroundContentsService::CreateBackgroundContents(
    SiteInstance* site,
    int routing_id,
    Profile* profile,
    const string16& frame_name,
    const string16& application_id) {
  BackgroundContents* contents = new BackgroundContents(site, routing_id, this);

  // Register the BackgroundContents internally, then send out a notification
  // to external listeners.
  BackgroundContentsOpenedDetails details = {contents,
                                             frame_name,
                                             application_id};
  BackgroundContentsOpened(&details);
  NotificationService::current()->Notify(
      NotificationType::BACKGROUND_CONTENTS_OPENED,
      Source<Profile>(profile),
      Details<BackgroundContentsOpenedDetails>(&details));
  return contents;
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
  DictionaryPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  DictionaryValue* pref = update.Get();
  const string16& appid = GetParentApplicationId(background_contents);
  DictionaryValue* current;
  if (pref->GetDictionaryWithoutPathExpansion(UTF16ToUTF8(appid), &current))
    return;

  // No entry for this application yet, so add one.
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kUrlKey, background_contents->GetURL().spec());
  dict->SetString(kFrameNameKey, contents_map_[appid].frame_name);
  pref->SetWithoutPathExpansion(UTF16ToUTF8(appid), dict);
  prefs_->ScheduleSavePersistentPrefs();
}

void BackgroundContentsService::UnregisterBackgroundContents(
    BackgroundContents* background_contents) {
  if (!prefs_)
    return;
  DCHECK(IsTracked(background_contents));
  const string16 appid = GetParentApplicationId(background_contents);
  DictionaryPrefUpdate update(prefs_, prefs::kRegisteredBackgroundContents);
  update.Get()->RemoveWithoutPathExpansion(UTF16ToUTF8(appid), NULL);
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

void BackgroundContentsService::AddTabContents(
    TabContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      new_contents->profile());
  if (!browser)
    return;
  browser->AddTabContents(new_contents, disposition, initial_pos, user_gesture);
}
