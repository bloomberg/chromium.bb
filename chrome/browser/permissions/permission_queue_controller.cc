// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_queue_controller.h"

#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_dialog_delegate.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace {

content::WebContents* GetWebContents(const PermissionRequestID& id) {
  return tab_util::GetWebContentsByFrameID(
      id.render_process_id(), id.render_frame_id());
}


InfoBarService* GetInfoBarService(const PermissionRequestID& id) {
  content::WebContents* web_contents = GetWebContents(id);
  return web_contents ? InfoBarService::FromWebContents(web_contents) : nullptr;
}

bool ArePermissionRequestsForSameTab(
    const PermissionRequestID& request,
    const PermissionRequestID& other_request) {
  content::WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      request.render_process_id(), request.render_frame_id());
  content::WebContents* other_web_contents = tab_util::GetWebContentsByFrameID(
      other_request.render_process_id(), other_request.render_frame_id());

  return web_contents == other_web_contents;
}

}  // anonymous namespace

class PermissionQueueController::PendingInfobarRequest {
 public:
  PendingInfobarRequest(ContentSettingsType type,
                        const PermissionRequestID& id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        bool user_gesture,
                        Profile* profile,
                        const PermissionDecidedCallback& callback);
  ~PendingInfobarRequest();

  bool IsForPair(const GURL& requesting_frame,
                 const GURL& embedder) const;

  PermissionRequestType request_type() const {
    return PermissionUtil::GetRequestType(type_);
  }

  PermissionRequestGestureType gesture_type() const {
    return PermissionUtil::GetGestureType(user_gesture_);
  }

  const PermissionRequestID& id() const { return id_; }
  const GURL& requesting_frame() const { return requesting_frame_; }
  bool has_gesture() const { return user_gesture_; }
  bool has_infobar() const { return !!infobar_; }
  bool has_dialog() const { return has_dialog_; }
  infobars::InfoBar* infobar() { return infobar_; }

  void RunCallback(ContentSetting content_setting);
  void CreatePrompt(PermissionQueueController* controller, bool show_dialog);

 private:
  ContentSettingsType type_;
  PermissionRequestID id_;
  GURL requesting_frame_;
  GURL embedder_;
  bool user_gesture_;
  Profile* profile_;
  PermissionDecidedCallback callback_;
  infobars::InfoBar* infobar_;
  bool has_dialog_;

  // Purposefully do not disable copying, as this is stored in STL containers.
};

PermissionQueueController::PendingInfobarRequest::PendingInfobarRequest(
    ContentSettingsType type,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool user_gesture,
    Profile* profile,
    const PermissionDecidedCallback& callback)
    : type_(type),
      id_(id),
      requesting_frame_(requesting_frame),
      embedder_(embedder),
      user_gesture_(user_gesture),
      profile_(profile),
      callback_(callback),
      infobar_(nullptr),
      has_dialog_(false) {}

PermissionQueueController::PendingInfobarRequest::~PendingInfobarRequest() {
}

bool PermissionQueueController::PendingInfobarRequest::IsForPair(
    const GURL& requesting_frame,
    const GURL& embedder) const {
  return (requesting_frame_ == requesting_frame) && (embedder_ == embedder);
}

void PermissionQueueController::PendingInfobarRequest::RunCallback(
    ContentSetting content_setting) {
  callback_.Run(content_setting);
}

void PermissionQueueController::PendingInfobarRequest::CreatePrompt(
    PermissionQueueController* controller,
    bool show_dialog) {
  // Controller can be Unretained because the lifetime of the infobar
  // is tied to that of the queue controller. Before QueueController
  // is destroyed, all requests will be cancelled and so all delegates
  // will be destroyed.
  PermissionInfoBarDelegate::PermissionSetCallback callback = base::Bind(
      &PermissionQueueController::OnPermissionSet, base::Unretained(controller),
      id_, requesting_frame_, embedder_, user_gesture_);

  if (show_dialog) {
    // We should show a dialog prompt instead of an infobar. Since only one
    // dialog can be shown at a time, the Java-side owns and manages the queue
    // of prompts; the bookkeeping in this class will work as expected:
    //   i. no pending request will ever have an infobar created for it.
    //  ii. OnPermissionSet is still called when the user makes a decision.
    has_dialog_ = true;
    PermissionDialogDelegate::Create(GetWebContents(id_), type_,
                                     requesting_frame_, user_gesture_, profile_,
                                     callback);
  } else {
    infobar_ = PermissionInfoBarDelegate::Create(
        GetInfoBarService(id_), type_, requesting_frame_, user_gesture_,
        profile_, callback);
  }
}

PermissionQueueController::PermissionQueueController(
    Profile* profile,
    ContentSettingsType content_settings_type)
    : profile_(profile),
      content_settings_type_(content_settings_type),
      in_shutdown_(false) {}

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
    bool user_gesture,
    const PermissionDecidedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (requesting_frame.SchemeIs(content::kChromeUIScheme) ||
      embedder.SchemeIs(content::kChromeUIScheme))
    return;

  pending_infobar_requests_.push_back(
      PendingInfobarRequest(content_settings_type_, id, requesting_frame,
                            embedder, user_gesture, profile_, callback));
  if (!AlreadyShowingInfoBarForTab(id))
    ShowQueuedInfoBarForTab(id);
}

void PermissionQueueController::CancelInfoBarRequest(
    const PermissionRequestID& id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  for (PendingInfobarRequests::iterator i(pending_infobar_requests_.begin());
       i != pending_infobar_requests_.end(); ++i) {
    if (id != i->id())
      continue;

    InfoBarService* infobar_service = GetInfoBarService(id);
    if (infobar_service && i->has_infobar())
      infobar_service->RemoveInfoBar(i->infobar());
    else
      pending_infobar_requests_.erase(i);
    return;
  }
}

void PermissionQueueController::OnPermissionSet(const PermissionRequestID& id,
                                                const GURL& requesting_frame,
                                                const GURL& embedder,
                                                bool user_gesture,
                                                bool update_content_setting,
                                                PermissionAction decision) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PermissionRequestType request_type =
      PermissionUtil::GetRequestType(content_settings_type_);
  PermissionRequestGestureType gesture_type =
      PermissionUtil::GetGestureType(user_gesture);
  PermissionEmbargoStatus embargo_status =
      PermissionEmbargoStatus::NOT_EMBARGOED;

  switch (decision) {
    case PermissionAction::GRANTED:
      PermissionUmaUtil::PermissionGranted(content_settings_type_, gesture_type,
                                           requesting_frame, profile_);
      PermissionUmaUtil::RecordPermissionPromptAccepted(request_type,
                                                        gesture_type);
      break;
    case PermissionAction::DENIED:
      PermissionUmaUtil::PermissionDenied(content_settings_type_, gesture_type,
                                          requesting_frame, profile_);
      PermissionUmaUtil::RecordPermissionPromptDenied(request_type,
                                                      gesture_type);
      break;
    case PermissionAction::DISMISSED:
      PermissionUmaUtil::PermissionDismissed(
          content_settings_type_, gesture_type, requesting_frame, profile_);
      if (PermissionDecisionAutoBlocker::GetForProfile(profile_)
              ->RecordDismissAndEmbargo(requesting_frame,
                                        content_settings_type_)) {
        embargo_status = PermissionEmbargoStatus::REPEATED_DISMISSALS;
      }
      break;
    default:
      NOTREACHED();
  }
  PermissionUmaUtil::RecordEmbargoStatus(embargo_status);

  // TODO(miguelg): move the permission persistence to
  // PermissionContextBase once all the types are moved there.
  if (update_content_setting)
    UpdateContentSetting(requesting_frame, embedder, decision);

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
    if (id == i->id()) {
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

  // PermissionContextBase needs to know about the new ContentSetting value,
  // CONTENT_SETTING_DEFAULT being the value for nothing happened. The callers
  // of ::OnPermissionSet passes { bool, GRANTED } for allow, { bool, DENIED }
  // for block and { false, DISMISSED } for dismissed. The tuple being
  // { update_content_setting, decision }.
  ContentSetting content_setting = CONTENT_SETTING_DEFAULT;
  if (decision == PermissionAction::GRANTED)
    content_setting = CONTENT_SETTING_ALLOW;
  else if (decision == PermissionAction::DENIED)
    content_setting = CONTENT_SETTING_BLOCK;
  else
    DCHECK_EQ(PermissionAction::DISMISSED, decision);

  // Send out the permission notifications.
  for (PendingInfobarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i) {
    i->RunCallback(content_setting);
  }

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
    if (ArePermissionRequestsForSameTab(i->id(), id) &&
        (i->has_infobar() || i->has_dialog())) {
      return true;
    }
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
    if (ArePermissionRequestsForSameTab(i->id(), id) && !i->has_infobar()) {
      // When using modal permission prompts, Java controls the display queue,
      // so infobar notifications are not relevant.
      bool show_dialog =
          PermissionDialogDelegate::ShouldShowDialog(i->has_gesture());
      if (!show_dialog)
        RegisterForInfoBarNotifications(infobar_service);

      PermissionUmaUtil::RecordPermissionPromptShown(i->request_type(),
                                                     i->gesture_type());
      i->CreatePrompt(this, show_dialog);
      return;
    }
  }

  UnregisterForInfoBarNotifications(infobar_service);
}

void PermissionQueueController::ClearPendingInfobarRequestsForTab(
    const PermissionRequestID& id) {
  for (PendingInfobarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (ArePermissionRequestsForSameTab(i->id(), id)) {
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
    PermissionAction decision) {
  DCHECK(decision == PermissionAction::GRANTED ||
         decision == PermissionAction::DENIED);
  if (requesting_frame.GetOrigin().SchemeIsFile()) {
    // Chrome can be launched with --disable-web-security which allows
    // geolocation requests from file:// URLs. We don't want to store these
    // in the host content settings map.
    return;
  }

  ContentSetting content_setting = (decision == PermissionAction::GRANTED)
                                       ? CONTENT_SETTING_ALLOW
                                       : CONTENT_SETTING_BLOCK;

  // TODO(timloh): Remove this logic when push and notification permissions
  // are reconciled, see crbug.com/563297.
  ContentSettingsType type_for_map = content_settings_type_;
  if (type_for_map == CONTENT_SETTINGS_TYPE_PUSH_MESSAGING)
    type_for_map = CONTENT_SETTINGS_TYPE_NOTIFICATIONS;
  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->SetContentSettingDefaultScope(
          requesting_frame.GetOrigin(), embedder.GetOrigin(),
          type_for_map, std::string(), content_setting);
}
