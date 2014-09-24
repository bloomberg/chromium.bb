// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/permission_queue_controller.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_context_uma_util.h"
#include "chrome/browser/geolocation/geolocation_infobar_delegate.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/midi_permission_infobar_delegate.h"
#include "chrome/browser/notifications/desktop_notification_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/services/gcm/push_messaging_infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

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


class PermissionQueueController::PendingInfobarRequest {
 public:
  PendingInfobarRequest(ContentSettingsType type,
                        const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        PermissionDecidedCallback callback);
  ~PendingInfobarRequest();

  bool IsForPair(const GURL& requesting_frame,
                 const GURL& embedder) const;

  const PermissionRequestID& id() const { return id_; }
  const GURL& requesting_frame() const { return requesting_frame_; }
  bool has_infobar() const { return !!infobar_; }
  infobars::InfoBar* infobar() { return infobar_; }

  void RunCallback(bool allowed);
  void CreateInfoBar(PermissionQueueController* controller,
                     const std::string& display_languages);

 private:
  ContentSettingsType type_;
  PermissionRequestID id_;
  GURL requesting_frame_;
  GURL embedder_;
  PermissionDecidedCallback callback_;
  infobars::InfoBar* infobar_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

PermissionQueueController::PendingInfobarRequest::PendingInfobarRequest(
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

PermissionQueueController::PendingInfobarRequest::~PendingInfobarRequest() {
}

bool PermissionQueueController::PendingInfobarRequest::IsForPair(
    const GURL& requesting_frame,
    const GURL& embedder) const {
  return (requesting_frame_ == requesting_frame) && (embedder_ == embedder);
}

void PermissionQueueController::PendingInfobarRequest::RunCallback(
    bool allowed) {
  callback_.Run(allowed);
}

void PermissionQueueController::PendingInfobarRequest::CreateInfoBar(
    PermissionQueueController* controller,
    const std::string& display_languages) {
  switch (type_) {
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
      infobar_ = GeolocationInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages);
      break;
#if defined(ENABLE_NOTIFICATIONS)
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
      infobar_ = DesktopNotificationInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages);
      break;
#endif  // ENABLE_NOTIFICATIONS
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
      infobar_ = MidiPermissionInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages, type_);
      break;
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:
      infobar_ = gcm::PushMessagingInfoBarDelegate::Create(
          GetInfoBarService(id_), controller, id_, requesting_frame_,
          display_languages, type_);
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

  if (requesting_frame.SchemeIs(content::kChromeUIScheme) ||
      embedder.SchemeIs(content::kChromeUIScheme))
    return;

  pending_infobar_requests_.push_back(PendingInfobarRequest(
      type_, id, requesting_frame, embedder, callback));
  if (!AlreadyShowingInfoBarForTab(id))
    ShowQueuedInfoBarForTab(id);
}

void PermissionQueueController::CancelInfoBarRequest(
    const PermissionRequestID& id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  for (PendingInfobarRequests::iterator i(pending_infobar_requests_.begin());
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

  // TODO(miguelg): move the permission persistence to
  // PermissionContextBase once all the types are moved there.
  if (update_content_setting) {
    UpdateContentSetting(requesting_frame, embedder, allowed);
    if (allowed)
      PermissionContextUmaUtil::PermissionGranted(type_);
    else
      PermissionContextUmaUtil::PermissionDenied(type_);
  } else {
    PermissionContextUmaUtil::PermissionDismissed(type_);
  }

  // Cancel this request first, then notify listeners.  TODO(pkasting): Why
  // is this order important?
  PendingInfobarRequests requests_to_notify;
  PendingInfobarRequests infobars_to_remove;
  std::vector<PendingInfobarRequests::iterator> pending_requests_to_remove;
  for (PendingInfobarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (!i->IsForPair(requesting_frame, embedder))
      continue;
    requests_to_notify.push_back(*i);
    if (!i->has_infobar()) {
      // We haven't created an infobar yet, just record the pending request
      // index and remove it later.
      pending_requests_to_remove.push_back(i);
      continue;
    }
    if (i->id().Equals(id)) {
      // The infobar that called us is i->infobar(), and its delegate is
      // currently in either Accept() or Cancel(). This means that
      // RemoveInfoBar() will be called later on, and that will trigger a
      // notification we're observing.
      continue;
    }

    // This infobar is for the same frame/embedder pair, but in a different
    // tab. We should remove it now that we've got an answer for it.
    infobars_to_remove.push_back(*i);
  }

  // Remove all infobars for the same |requesting_frame| and |embedder|.
  for (PendingInfobarRequests::iterator i = infobars_to_remove.begin();
       i != infobars_to_remove.end(); ++i)
    GetInfoBarService(i->id())->RemoveInfoBar(i->infobar());

  // Send out the permission notifications.
  for (PendingInfobarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i)
    i->RunCallback(allowed);

  // Remove the pending requests in reverse order.
  for (int i = pending_requests_to_remove.size() - 1; i >= 0; --i)
    pending_infobar_requests_.erase(pending_requests_to_remove[i]);
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
  // use to find the PendingInfobarRequest to remove because
  // pending_infobar_requests_ will not have received any new entries between
  // the NotificationService's call to InfoBarContainer::Observe and this
  // method.
  infobars::InfoBar* infobar =
      content::Details<infobars::InfoBar::RemovedDetails>(details)->first;
  for (PendingInfobarRequests::iterator i = pending_infobar_requests_.begin();
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
  for (PendingInfobarRequests::const_iterator i(
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
    ClearPendingInfobarRequestsForTab(id);
    return;
  }

  for (PendingInfobarRequests::iterator i = pending_infobar_requests_.begin();
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

void PermissionQueueController::ClearPendingInfobarRequestsForTab(
    const PermissionRequestID& id) {
  for (PendingInfobarRequests::iterator i = pending_infobar_requests_.begin();
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

  ContentSettingsPattern embedder_pattern =
      (type_ == CONTENT_SETTINGS_TYPE_NOTIFICATIONS) ?
      ContentSettingsPattern::Wildcard() :
      ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin());

  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
      embedder_pattern,
      type_,
      std::string(),
      content_setting);
}
