// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_queue_controller.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"


// Utilities ------------------------------------------------------------------

namespace {

InfoBarService* GetInfoBarService(const GeolocationPermissionRequestID& id) {
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(id.render_process_id(), id.render_view_id());
  return web_contents ? InfoBarService::FromWebContents(web_contents) : NULL;
}

}


// GeolocationInfoBarQueueController::PendingInfoBarRequest -------------------

class GeolocationInfoBarQueueController::PendingInfoBarRequest {
 public:
  PendingInfoBarRequest(const GeolocationPermissionRequestID& id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        PermissionDecidedCallback callback);
  ~PendingInfoBarRequest();

  bool IsForPair(const GURL& requesting_frame,
                 const GURL& embedder) const;

  const GeolocationPermissionRequestID& id() const { return id_; }
  const GURL& requesting_frame() const { return requesting_frame_; }
  bool has_infobar_delegate() const { return !!infobar_delegate_; }
  InfoBarDelegate* infobar_delegate() { return infobar_delegate_; }

  void RunCallback(bool allowed);
  void CreateInfoBarDelegate(GeolocationInfoBarQueueController* controller,
                             const std::string& display_languages);

 private:
  GeolocationPermissionRequestID id_;
  GURL requesting_frame_;
  GURL embedder_;
  PermissionDecidedCallback callback_;
  InfoBarDelegate* infobar_delegate_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

GeolocationInfoBarQueueController::PendingInfoBarRequest::PendingInfoBarRequest(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    PermissionDecidedCallback callback)
    : id_(id),
      requesting_frame_(requesting_frame),
      embedder_(embedder),
      callback_(callback),
      infobar_delegate_(NULL) {
}

GeolocationInfoBarQueueController::PendingInfoBarRequest::
    ~PendingInfoBarRequest() {
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::IsForPair(
    const GURL& requesting_frame,
    const GURL& embedder) const {
  return (requesting_frame_ == requesting_frame) && (embedder_ == embedder);
}

void GeolocationInfoBarQueueController::PendingInfoBarRequest::RunCallback(
    bool allowed) {
  callback_.Run(allowed);
}

void GeolocationInfoBarQueueController::PendingInfoBarRequest::
    CreateInfoBarDelegate(GeolocationInfoBarQueueController* controller,
                          const std::string& display_languages) {
  infobar_delegate_ = GeolocationConfirmInfoBarDelegate::Create(
      GetInfoBarService(id_), controller, id_, requesting_frame_,
      display_languages);

}


// GeolocationInfoBarQueueController ------------------------------------------

GeolocationInfoBarQueueController::GeolocationInfoBarQueueController(
    Profile* profile)
    : profile_(profile) {
}

GeolocationInfoBarQueueController::~GeolocationInfoBarQueueController() {
}

void GeolocationInfoBarQueueController::CreateInfoBarRequest(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    PermissionDecidedCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // We shouldn't get duplicate requests.
  for (PendingInfoBarRequests::const_iterator i(
           pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i)
    DCHECK(!i->id().Equals(id));

  pending_infobar_requests_.push_back(PendingInfoBarRequest(
      id, requesting_frame, embedder, callback));
  if (!AlreadyShowingInfoBarForTab(id))
    ShowQueuedInfoBarForTab(id);
}

void GeolocationInfoBarQueueController::CancelInfoBarRequest(
    const GeolocationPermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  for (PendingInfoBarRequests::iterator i(pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().Equals(id)) {
      if (i->has_infobar_delegate())
        GetInfoBarService(id)->RemoveInfoBar(i->infobar_delegate());
      else
        pending_infobar_requests_.erase(i);
      return;
    }
  }
}

void GeolocationInfoBarQueueController::OnPermissionSet(
    const GeolocationPermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool update_content_setting,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (update_content_setting) {
    ContentSetting content_setting =
        allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    profile_->GetHostContentSettingsMap()->SetContentSetting(
        ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
        ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin()),
        CONTENT_SETTINGS_TYPE_GEOLOCATION,
        std::string(),
        content_setting);
  }

  // Cancel this request first, then notify listeners.  TODO(pkasting): Why
  // is this order important?
  PendingInfoBarRequests requests_to_notify;
  PendingInfoBarRequests infobars_to_remove;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForPair(requesting_frame, embedder)) {
      requests_to_notify.push_back(*i);
      if (i->id().Equals(id)) {
        // The delegate that called us is i, and it's currently in either
        // Accept() or Cancel(). This means that the owning InfoBar will call
        // RemoveInfoBar() later on, and that will trigger a notification we're
        // observing.
        ++i;
      } else if (i->has_infobar_delegate()) {
        // This InfoBar is for the same frame/embedder pair, but in a different
        // tab. We should remove it now that we've got an answer for it.
        infobars_to_remove.push_back(*i);
        ++i;
      } else {
        // We haven't created an InfoBar yet, just remove the pending request.
        i = pending_infobar_requests_.erase(i);
      }
    } else {
      ++i;
    }
  }

  // Remove all InfoBars for the same |requesting_frame| and |embedder|.
  for (PendingInfoBarRequests::iterator i = infobars_to_remove.begin();
       i != infobars_to_remove.end(); ++i)
    GetInfoBarService(i->id())->RemoveInfoBar(i->infobar_delegate());

  // Send out the permission notifications.
  for (PendingInfoBarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i)
    i->RunCallback(allowed);
}

void GeolocationInfoBarQueueController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  // We will receive this notification for all infobar closures, so we need to
  // check whether this is the geolocation infobar we're tracking. Note that the
  // InfoBarContainer (if any) may have received this notification before us and
  // caused the delegate to be deleted, so it's not safe to dereference the
  // contents of the delegate. The address of the delegate, however, is OK to
  // use to find the PendingInfoBarRequest to remove because
  // pending_infobar_requests_ will not have received any new entries between
  // the NotificationService's call to InfoBarContainer::Observe and this
  // method.
  InfoBarDelegate* delegate =
      content::Details<InfoBarRemovedDetails>(details)->first;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    InfoBarDelegate* confirm_delegate = i->infobar_delegate();
    if (confirm_delegate == delegate) {
      GeolocationPermissionRequestID id(i->id());
      pending_infobar_requests_.erase(i);
      ShowQueuedInfoBarForTab(id);
      return;
    }
  }
}

bool GeolocationInfoBarQueueController::AlreadyShowingInfoBarForTab(
    const GeolocationPermissionRequestID& id) const {
  for (PendingInfoBarRequests::const_iterator i(
           pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().IsForSameTabAs(id) && i->has_infobar_delegate())
      return true;
  }
  return false;
}

void GeolocationInfoBarQueueController::ShowQueuedInfoBarForTab(
    const GeolocationPermissionRequestID& id) {
  DCHECK(!AlreadyShowingInfoBarForTab(id));

  InfoBarService* infobar_service = GetInfoBarService(id);
  if (!infobar_service) {
    // We can get here for example during tab shutdown, when the
    // InfoBarService is removing all existing infobars, thus calling back to
    // Observe().  In this case the service still exists, and is supplied as the
    // source of the notification we observed, but is no longer accessible from
    // its WebContents.  In this case we should just go ahead and cancel further
    // infobars for this tab instead of trying to access the service.
    ClearPendingInfoBarRequestsForTab(id);
    return;
  }

  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().IsForSameTabAs(id) && !i->has_infobar_delegate()) {
      RegisterForInfoBarNotifications(infobar_service);
      i->CreateInfoBarDelegate(
          this, profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
      return;
    }
  }

  UnregisterForInfoBarNotifications(infobar_service);
}

void GeolocationInfoBarQueueController::ClearPendingInfoBarRequestsForTab(
    const GeolocationPermissionRequestID& id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->id().IsForSameTabAs(id))
      i = pending_infobar_requests_.erase(i);
    else
      ++i;
  }
}

void GeolocationInfoBarQueueController::RegisterForInfoBarNotifications(
    InfoBarService* infobar_service) {
  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(infobar_service))) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   content::Source<InfoBarService>(infobar_service));
  }
}

void GeolocationInfoBarQueueController::UnregisterForInfoBarNotifications(
    InfoBarService* infobar_service) {
  if (registrar_.IsRegistered(
      this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(infobar_service))) {
    registrar_.Remove(this,
                      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                      content::Source<InfoBarService>(infobar_service));
  }
}
