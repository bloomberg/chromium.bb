// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_queue_controller.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/midi_permission_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/media/protected_media_identifier_infobar_delegate.h"
#endif

namespace {

InfoBarService* GetInfoBarService(const PermissionRequestID& id) {
  content::WebContents* web_contents =
      tab_util::GetWebContentsByID(id.render_process_id(), id.render_view_id());
  return web_contents ? InfoBarService::FromWebContents(web_contents) : NULL;
}

}


class PermissionQueueController::PendingInfoBarRequest {
 public:
  PendingInfoBarRequest(ContentSettingsType type,
                        const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        PermissionDecidedCallback callback);
  ~PendingInfoBarRequest();

  bool IsForPair(const GURL& requesting_frame,
                 const GURL& embedder) const;

  const PermissionRequestID& id() const { return id_; }
  const GURL& requesting_frame() const { return requesting_frame_; }
  bool has_infobar() const { return !!infobar_; }
  InfoBarDelegate* infobar() { return infobar_; }

  void RunCallback(bool allowed);
  void CreateInfoBar(PermissionQueueController* controller,
                     const std::string& display_languages);

 private:
  ContentSettingsType type_;
  PermissionRequestID id_;
  GURL requesting_frame_;
  GURL embedder_;
  PermissionDecidedCallback callback_;
  InfoBarDelegate* infobar_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

PermissionQueueController::PendingInfoBarRequest::PendingInfoBarRequest(
    ContentSettingsType type,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    PermissionDecidedCallback callback)
    : type_(type),
      id_(id),
      requesting_frame_(requesting_frame),
      embedder_(embedder),
      callback_(callback),
      infobar_(NULL) {
}

PermissionQueueController::PendingInfoBarRequest::~PendingInfoBarRequest() {
}

bool PermissionQueueController::PendingInfoBarRequest::IsForPair(
    const GURL& requesting_frame,
    const GURL& embedder) const {
  return (requesting_frame_ == requesting_frame) && (embedder_ == embedder);
}

void PermissionQueueController::PendingInfoBarRequest::RunCallback(
    bool allowed) {
  callback_.Run(allowed);
}

void PermissionQueueController::PendingInfoBarRequest::CreateInfoBar(
    PermissionQueueController* controller,
    const std::string& display_languages) {
  // TODO(toyoshim): Remove following ContentType dependent code.
  // Also these InfoBarDelegate can share much more code each other.
  // http://crbug.com/266743
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      infobar_ = GeolocationInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages);
      break;
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      infobar_ = MIDIPermissionInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages);
      break;
#if defined(OS_ANDROID)
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:
      infobar_ = ProtectedMediaIdentifierInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages);
      break;
#endif
    default:
      NOTREACHED();
      break;
  }
}


PermissionQueueController::PermissionQueueController(Profile* profile,
                                                     ContentSettingsType type)
    : profile_(profile),
      type_(type),
      in_shutdown_(false) {
}

PermissionQueueController::~PermissionQueueController() {
  // Cancel all outstanding requests.
  in_shutdown_ = true;
  while (!pending_infobar_requests_.empty())
    CancelInfoBarRequest(pending_infobar_requests_.front().id());
}

void PermissionQueueController::CreateInfoBarRequest(
    const PermissionRequestID& id,
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
      type_, id, requesting_frame, embedder, callback));
  if (!AlreadyShowingInfoBarForTab(id))
    ShowQueuedInfoBarForTab(id);
}

void PermissionQueueController::CancelInfoBarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  for (PendingInfoBarRequests::iterator i(pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().Equals(id)) {
      if (i->has_infobar())
        GetInfoBarService(id)->RemoveInfoBar(i->infobar());
      else
        pending_infobar_requests_.erase(i);
      return;
    }
  }
}

void PermissionQueueController::OnPermissionSet(
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool update_content_setting,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (update_content_setting)
    UpdateContentSetting(requesting_frame, embedder, allowed);

  // Cancel this request first, then notify listeners.  TODO(pkasting): Why
  // is this order important?
  PendingInfoBarRequests requests_to_notify;
  PendingInfoBarRequests infobars_to_remove;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForPair(requesting_frame, embedder)) {
      requests_to_notify.push_back(*i);
      if (i->id().Equals(id)) {
        // The infobar that called us is i->infobar(), and it's currently in
        // either Accept() or Cancel(). This means that RemoveInfoBar() will be
        // called later on, and that will trigger a notification we're
        // observing.
        ++i;
      } else if (i->has_infobar()) {
        // This infobar is for the same frame/embedder pair, but in a different
        // tab. We should remove it now that we've got an answer for it.
        infobars_to_remove.push_back(*i);
        ++i;
      } else {
        // We haven't created an infobar yet, just remove the pending request.
        i = pending_infobar_requests_.erase(i);
      }
    } else {
      ++i;
    }
  }

  // Remove all infobars for the same |requesting_frame| and |embedder|.
  for (PendingInfoBarRequests::iterator i = infobars_to_remove.begin();
       i != infobars_to_remove.end(); ++i)
    GetInfoBarService(i->id())->RemoveInfoBar(i->infobar());

  // Send out the permission notifications.
  for (PendingInfoBarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i)
    i->RunCallback(allowed);
}

void PermissionQueueController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
  // We will receive this notification for all infobar closures, so we need to
  // check whether this is the geolocation infobar we're tracking. Note that the
  // InfoBarContainer (if any) may have received this notification before us and
  // caused the infobar to be deleted, so it's not safe to dereference the
  // contents of the infobar. The address of the infobar, however, is OK to
  // use to find the PendingInfoBarRequest to remove because
  // pending_infobar_requests_ will not have received any new entries between
  // the NotificationService's call to InfoBarContainer::Observe and this
  // method.
  InfoBarDelegate* infobar =
      content::Details<InfoBarRemovedDetails>(details)->first;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->infobar() == infobar) {
      PermissionRequestID id(i->id());
      pending_infobar_requests_.erase(i);
      ShowQueuedInfoBarForTab(id);
      return;
    }
  }
}

bool PermissionQueueController::AlreadyShowingInfoBarForTab(
    const PermissionRequestID& id) const {
  for (PendingInfoBarRequests::const_iterator i(
           pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().IsForSameTabAs(id) && i->has_infobar())
      return true;
  }
  return false;
}

void PermissionQueueController::ShowQueuedInfoBarForTab(
    const PermissionRequestID& id) {
  DCHECK(!AlreadyShowingInfoBarForTab(id));

  // We can get here for example during tab shutdown, when the InfoBarService is
  // removing all existing infobars, thus calling back to Observe(). In this
  // case the service still exists, and is supplied as the source of the
  // notification we observed, but is no longer accessible from its WebContents.
  // In this case we should just go ahead and cancel further infobars for this
  // tab instead of trying to access the service.
  //
  // Similarly, if we're being destroyed, we should also avoid showing further
  // infobars.
  InfoBarService* infobar_service = GetInfoBarService(id);
  if (!infobar_service || in_shutdown_) {
    ClearPendingInfoBarRequestsForTab(id);
    return;
  }

  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->id().IsForSameTabAs(id) && !i->has_infobar()) {
      RegisterForInfoBarNotifications(infobar_service);
      i->CreateInfoBar(
          this, profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
      return;
    }
  }

  UnregisterForInfoBarNotifications(infobar_service);
}

void PermissionQueueController::ClearPendingInfoBarRequestsForTab(
    const PermissionRequestID& id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->id().IsForSameTabAs(id)) {
      DCHECK(!i->has_infobar());
      i = pending_infobar_requests_.erase(i);
    } else {
      ++i;
    }
  }
}

void PermissionQueueController::RegisterForInfoBarNotifications(
    InfoBarService* infobar_service) {
  if (!registrar_.IsRegistered(
      this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(infobar_service))) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   content::Source<InfoBarService>(infobar_service));
  }
}

void PermissionQueueController::UnregisterForInfoBarNotifications(
    InfoBarService* infobar_service) {
  if (registrar_.IsRegistered(
      this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::Source<InfoBarService>(infobar_service))) {
    registrar_.Remove(this,
                      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                      content::Source<InfoBarService>(infobar_service));
  }
}

void PermissionQueueController::UpdateContentSetting(
    const GURL& requesting_frame,
    const GURL& embedder,
    bool allowed) {
  if (requesting_frame.GetOrigin().SchemeIsFile()) {
    // Chrome can be launched with --disable-web-security which allows
    // geolocation requests from file:// URLs. We don't want to store these
    // in the host content settings map.
    return;
  }

  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
      ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin()),
      type_,
      std::string(),
      content_setting);
}
