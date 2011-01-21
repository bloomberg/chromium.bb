// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_provider.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// GeolocationInfoBarQueueController ------------------------------------------

// This class controls the geolocation infobar queue per profile, and it's an
// internal class to GeolocationPermissionContext.
// An alternate approach would be to have this queue per tab, and use
// notifications to broadcast when permission is set / listen to notification to
// cancel pending requests. This may be specially useful if there are other
// things listening for such notifications.
// For the time being this class is self-contained and it doesn't seem pulling
// the notification infrastructure would simplify.
class GeolocationInfoBarQueueController : NotificationObserver {
 public:
  GeolocationInfoBarQueueController(
      GeolocationPermissionContext* geolocation_permission_context,
      Profile* profile);
  ~GeolocationInfoBarQueueController();

  // The InfoBar will be displayed immediately if the tab is not already
  // displaying one, otherwise it'll be queued.
  void CreateInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id,
                            const GURL& requesting_frame,
                            const GURL& emebedder);

  // Cancels a specific infobar request.
  void CancelInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id);

  // Called by the InfoBarDelegate to notify it's closed. It'll display a new
  // InfoBar if there's any request pending for this tab.
  void OnInfoBarClosed(int render_process_id,
                       int render_view_id,
                       int bridge_id);

  // Called by the InfoBarDelegate to notify permission has been set.
  // It'll notify and dismiss any other pending InfoBar request for the same
  // |requesting_frame| and embedder.
  void OnPermissionSet(int render_process_id,
                       int render_view_id,
                       int bridge_id,
                       const GURL& requesting_frame,
                       const GURL& embedder,
                       bool allowed);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  struct PendingInfoBarRequest;
  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;

  // Shows the first pending infobar for this tab.
  void ShowQueuedInfoBar(int render_process_id, int render_view_id);

  // Cancels an InfoBar request and returns the next iterator position.
  std::vector<PendingInfoBarRequest>::iterator CancelInfoBarRequestInternal(
      std::vector<PendingInfoBarRequest>::iterator i);

  NotificationRegistrar registrar_;

  GeolocationPermissionContext* const geolocation_permission_context_;
  Profile* const profile_;
  // Contains all pending infobar requests.
  PendingInfoBarRequests pending_infobar_requests_;
};

namespace {

const char kGeolocationLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
    "http://www.google.com/support/chromeos/bin/answer.py?answer=142065";
#else
    "http://www.google.com/support/chrome/bin/answer.py?answer=142065";
#endif


// GeolocationConfirmInfoBarDelegate ------------------------------------------

class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegate(
      TabContents* tab_contents,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

 private:
  virtual ~GeolocationConfirmInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual void InfoBarClosed();
  virtual SkBitmap* GetIcon() const;
  virtual Type GetInfoBarType() const;
  virtual string16 GetMessageText() const;
  virtual int GetButtons() const;
  virtual string16 GetButtonLabel(InfoBarButton button) const;
  virtual bool Accept();
  virtual bool Cancel();
  virtual string16 GetLinkText();
  virtual bool LinkClicked(WindowOpenDisposition disposition);

  TabContents* tab_contents_;
  GeolocationInfoBarQueueController* controller_;
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;
  GURL requesting_frame_url_;
  std::string display_languages_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

GeolocationConfirmInfoBarDelegate::GeolocationConfirmInfoBarDelegate(
    TabContents* tab_contents,
    GeolocationInfoBarQueueController* controller,
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame_url,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(tab_contents),
      tab_contents_(tab_contents),
      controller_(controller),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id),
      requesting_frame_url_(requesting_frame_url),
      display_languages_(display_languages) {
}

GeolocationConfirmInfoBarDelegate::~GeolocationConfirmInfoBarDelegate() {
}

void GeolocationConfirmInfoBarDelegate::InfoBarClosed() {
  controller_->OnInfoBarClosed(render_process_id_, render_view_id_,
                               bridge_id_);
  delete this;
}

SkBitmap* GeolocationConfirmInfoBarDelegate::GetIcon() const {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_GEOLOCATION_INFOBAR_ICON);
}

InfoBarDelegate::Type
    GeolocationConfirmInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

string16 GeolocationConfirmInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_GEOLOCATION_INFOBAR_QUESTION,
      net::FormatUrl(requesting_frame_url_.GetOrigin(), display_languages_));
}

int GeolocationConfirmInfoBarDelegate::GetButtons() const {
  return BUTTON_OK | BUTTON_CANCEL;
}

string16 GeolocationConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}

bool GeolocationConfirmInfoBarDelegate::Accept() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, tab_contents_->GetURL(), true);
  return true;
}

bool GeolocationConfirmInfoBarDelegate::Cancel() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, tab_contents_->GetURL(), false);
  return true;
}

string16 GeolocationConfirmInfoBarDelegate::GetLinkText() {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GeolocationConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  // Ignore the click disposition and always open in a new top level tab.
  tab_contents_->OpenURL(
      google_util::AppendGoogleLocaleParam(GURL(kGeolocationLearnMoreUrl)),
      GURL(), NEW_FOREGROUND_TAB, PageTransition::LINK);
  return false;  // Do not dismiss the info bar.
}

}  // namespace


// GeolocationInfoBarQueueController ------------------------------------------

struct GeolocationInfoBarQueueController::PendingInfoBarRequest {
  int render_process_id;
  int render_view_id;
  int bridge_id;
  GURL requesting_frame;
  GURL embedder;
  // If non-NULL, it's the current geolocation infobar for this tab.
  InfoBarDelegate* infobar_delegate;

  bool IsForTab(int p_render_process_id, int p_render_view_id) const {
    return render_process_id == p_render_process_id &&
           render_view_id == p_render_view_id;
  }

  bool IsForPair(const GURL& p_requesting_frame, const GURL& p_embedder) const {
    return requesting_frame == p_requesting_frame &&
           embedder == p_embedder;
  }

  bool Equals(int p_render_process_id,
              int p_render_view_id,
              int p_bridge_id) const {
    return IsForTab(p_render_process_id, p_render_view_id) &&
           bridge_id == p_bridge_id;
  }
};

GeolocationInfoBarQueueController::GeolocationInfoBarQueueController(
    GeolocationPermissionContext* geolocation_permission_context,
    Profile* profile)
    : geolocation_permission_context_(geolocation_permission_context),
      profile_(profile) {
}

GeolocationInfoBarQueueController::~GeolocationInfoBarQueueController() {
}

void GeolocationInfoBarQueueController::CreateInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder) {
  // This makes sure that no duplicates are added to
  // |pending_infobar_requests_| as an artificial permission request may
  // already exist in the queue as per
  // GeolocationPermissionContext::StartUpdatingRequested
  // See http://crbug.com/51899 for more details.
  // TODO(joth): Once we have CLIENT_BASED_GEOLOCATION and
  // WTF_USE_PREEMPT_GEOLOCATION_PERMISSION set in WebKit we should be able to
  // just use a DCHECK to check if a duplicate is attempting to be added.
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->Equals(render_process_id, render_view_id, bridge_id)) {
      // The request already exists.
      DCHECK(i->IsForPair(requesting_frame, embedder));
      return;
    }
  }
  PendingInfoBarRequest pending_infobar_request;
  pending_infobar_request.render_process_id = render_process_id;
  pending_infobar_request.render_view_id = render_view_id;
  pending_infobar_request.bridge_id = bridge_id;
  pending_infobar_request.requesting_frame = requesting_frame;
  pending_infobar_request.embedder = embedder;
  pending_infobar_request.infobar_delegate = NULL;
  pending_infobar_requests_.push_back(pending_infobar_request);
  ShowQueuedInfoBar(render_process_id, render_view_id);
}

void GeolocationInfoBarQueueController::CancelInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->Equals(render_process_id, render_view_id, bridge_id)) {
      CancelInfoBarRequestInternal(i);
      return;
    }
  }
}

void GeolocationInfoBarQueueController::OnInfoBarClosed(int render_process_id,
                                                        int render_view_id,
                                                        int bridge_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->Equals(render_process_id, render_view_id, bridge_id)) {
      pending_infobar_requests_.erase(i);
      break;
    }
  }
  ShowQueuedInfoBar(render_process_id, render_view_id);
}

void GeolocationInfoBarQueueController::OnPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Persist the permission.
  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetGeolocationContentSettingsMap()->SetContentSetting(
      requesting_frame.GetOrigin(), embedder.GetOrigin(), content_setting);

  // Now notify all pending requests that the permission has been set.
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end();) {
    if (i->IsForPair(requesting_frame, embedder)) {
      // There was a pending request for the same [frame, embedder].
      if (i->Equals(render_process_id, render_view_id, bridge_id)) {
        // The request that set permission will be removed by TabContents
        // itself, that is, we should not try to cancel the infobar that has
        // just notified us.
        i->infobar_delegate = NULL;
      }
      // Cancel it first, and then notify the permission.
      // Note: if the pending request had an infobar, TabContents will
      // eventually close it and we will pump the queue via OnInfoBarClosed().
      PendingInfoBarRequest copied_request = *i;
      i = CancelInfoBarRequestInternal(i);
      geolocation_permission_context_->NotifyPermissionSet(
          copied_request.render_process_id, copied_request.render_view_id,
          copied_request.bridge_id, copied_request.requesting_frame, allowed);
    } else {
      ++i;
    }
  }
}

void GeolocationInfoBarQueueController::Observe(
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    source);
  TabContents* tab_contents = Source<TabContents>(source).ptr();
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end();) {
    if (i->infobar_delegate == NULL &&
        tab_contents == tab_util::GetTabContentsByID(i->render_process_id,
                                                     i->render_view_id)) {
      i = pending_infobar_requests_.erase(i);
    } else {
      ++i;
    }
  }
}

void GeolocationInfoBarQueueController::ShowQueuedInfoBar(int render_process_id,
                                                          int render_view_id) {
  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end();) {
    if (!i->IsForTab(render_process_id, render_view_id)) {
      ++i;
      continue;
    }
    if (!tab_contents) {
      i = pending_infobar_requests_.erase(i);
      continue;
    }
    // Check if already displayed.
    if (i->infobar_delegate)
      break;
    if (!registrar_.IsRegistered(this, NotificationType::TAB_CONTENTS_DESTROYED,
        Source<TabContents>(tab_contents))) {
      registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
          Source<TabContents>(tab_contents));
    }
    i->infobar_delegate = new GeolocationConfirmInfoBarDelegate(tab_contents,
        this, render_process_id, render_view_id, i->bridge_id,
        i->requesting_frame,
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
    tab_contents->AddInfoBar(i->infobar_delegate);
    break;
  }
}

std::vector<GeolocationInfoBarQueueController::PendingInfoBarRequest>::iterator
    GeolocationInfoBarQueueController::CancelInfoBarRequestInternal(
        std::vector<PendingInfoBarRequest>::iterator i) {
  TabContents* tab_contents =
      tab_util::GetTabContentsByID(i->render_process_id, i->render_view_id);
  if (tab_contents && i->infobar_delegate) {
    // TabContents will destroy the InfoBar, which will remove from our vector
    // asynchronously.
    tab_contents->RemoveInfoBar(i->infobar_delegate);
    return ++i;
  } else {
    // Remove it directly from the pending vector.
    return pending_infobar_requests_.erase(i);
  }
}

GeolocationPermissionContext::GeolocationPermissionContext(
    Profile* profile)
    : profile_(profile),
      ALLOW_THIS_IN_INITIALIZER_LIST(geolocation_infobar_queue_controller_(
         new GeolocationInfoBarQueueController(this, profile))) {
}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
        this, &GeolocationPermissionContext::RequestGeolocationPermission,
        render_process_id, render_view_id, bridge_id, requesting_frame));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* extensions = profile_->GetExtensionService();
  if (extensions) {
    const Extension* ext = extensions->GetExtensionByURL(requesting_frame);
    if (!ext)
      ext = extensions->GetExtensionByWebExtent(requesting_frame);
    if (ext && ext->HasApiPermission(Extension::kGeolocationPermission)) {
      ExtensionProcessManager* epm = profile_->GetExtensionProcessManager();
      RenderProcessHost* process = epm->GetExtensionProcess(requesting_frame);
      if (process && process->id() == render_process_id) {
        NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                            requesting_frame, true);
        return;
      }
    }
  }

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  if (!tab_contents) {
    // The tab may have gone away, or the request may not be from a tab at all.
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << render_process_id << "," << render_view_id << ","
                 << bridge_id << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, false);
    return;
  }

  GURL embedder = tab_contents->GetURL();
  if (!requesting_frame.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use geolocation from an invalid URL: "
                 << requesting_frame << "," << embedder
                 << " (geolocation is not supported in popups)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, false);
    return;
  }

  ContentSetting content_setting =
      profile_->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame, embedder);
  if (content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, false);
  } else if (content_setting == CONTENT_SETTING_ALLOW) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, true);
  } else {  // setting == ask. Prompt the user.
    geolocation_infobar_queue_controller_->CreateInfoBarRequest(
        render_process_id, render_view_id, bridge_id, requesting_frame,
        embedder);
  }
}

void GeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfoBarRequest(render_process_id, render_view_id, bridge_id);
}

void GeolocationPermissionContext::NotifyPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);

  // TabContents may have gone away (or not exists for extension).
  if (tab_contents) {
    TabSpecificContentSettings* content_settings =
        tab_contents->GetTabSpecificContentSettings();
    content_settings->OnGeolocationPermissionSet(requesting_frame.GetOrigin(),
                                                 allowed);
  }

  CallRenderViewHost(render_process_id, render_view_id, &RenderViewHost::Send,
      new ViewMsg_Geolocation_PermissionSet(render_view_id, bridge_id,
                                            allowed));
  if (allowed) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, NewRunnableMethod(
        this, &GeolocationPermissionContext::NotifyArbitratorPermissionGranted,
        requesting_frame));
  }
}

void GeolocationPermissionContext::NotifyArbitratorPermissionGranted(
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProvider::GetInstance()->OnPermissionGranted(requesting_frame);
}

void GeolocationPermissionContext::CancelPendingInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
        this, &GeolocationPermissionContext::CancelPendingInfoBarRequest,
        render_process_id, render_view_id, bridge_id));
     return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  geolocation_infobar_queue_controller_->CancelInfoBarRequest(render_process_id,
      render_view_id, bridge_id);
}
