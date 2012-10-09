// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_infobar_queue_controller.h"

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

// GeolocationConfirmInfoBarDelegate ------------------------------------------

namespace {

class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }

 private:

  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  GeolocationInfoBarQueueController* controller_;
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;

  GURL requesting_frame_url_;
  std::string display_languages_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

GeolocationConfirmInfoBarDelegate::GeolocationConfirmInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    GeolocationInfoBarQueueController* controller,
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame_url,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(infobar_helper),
      controller_(controller),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id),
      requesting_frame_url_(requesting_frame_url),
      display_languages_(display_languages) {
  const NavigationEntry* committed_entry =
      infobar_helper->GetWebContents()->GetController().GetLastCommittedEntry();
  set_contents_unique_id(committed_entry ? committed_entry->GetUniqueID() : 0);
}

gfx::Image* GeolocationConfirmInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(
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

string16 GeolocationConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_GEOLOCATION_ALLOW_BUTTON : IDS_GEOLOCATION_DENY_BUTTON);
}

bool GeolocationConfirmInfoBarDelegate::Accept() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, owner()->GetWebContents()->GetURL(), true);
  return true;
}

bool GeolocationConfirmInfoBarDelegate::Cancel() {
  controller_->OnPermissionSet(render_process_id_, render_view_id_, bridge_id_,
      requesting_frame_url_, owner()->GetWebContents()->GetURL(),
      false);
  return true;
}

string16 GeolocationConfirmInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool GeolocationConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  const char kGeolocationLearnMoreUrl[] =
#if defined(OS_CHROMEOS)
      "https://www.google.com/support/chromeos/bin/answer.py?answer=142065";
#else
      "https://www.google.com/support/chrome/bin/answer.py?answer=142065";
#endif

  OpenURLParams params(
      google_util::AppendGoogleLocaleParam(GURL(kGeolocationLearnMoreUrl)),
      Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;  // Do not dismiss the info bar.
}

}  // namespace


// GeolocationInfoBarQueueController::PendingInfoBarRequest -------------------

struct GeolocationInfoBarQueueController::PendingInfoBarRequest {
 public:
  PendingInfoBarRequest(int render_process_id,
                        int render_view_id,
                        int bridge_id,
                        const GURL& requesting_frame,
                        const GURL& embedder,
                        base::Callback<void(bool)> callback);

  bool IsForTab(int p_render_process_id, int p_render_view_id) const;
  bool IsForPair(const GURL& p_requesting_frame,
                 const GURL& p_embedder) const;
  bool Equals(int p_render_process_id,
              int p_render_view_id,
              int p_bridge_id) const;

  int render_process_id;
  int render_view_id;
  int bridge_id;
  GURL requesting_frame;
  GURL embedder;
  base::Callback<void(bool)> callback;
  GeolocationConfirmInfoBarDelegate* infobar_delegate;
};

GeolocationInfoBarQueueController::PendingInfoBarRequest::PendingInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback)
    : render_process_id(render_process_id),
      render_view_id(render_view_id),
      bridge_id(bridge_id),
      requesting_frame(requesting_frame),
      embedder(embedder),
      callback(callback),
      infobar_delegate(NULL) {
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::IsForTab(
    int p_render_process_id,
    int p_render_view_id) const {
  return (render_process_id == p_render_process_id) &&
      (render_view_id == p_render_view_id);
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::IsForPair(
    const GURL& p_requesting_frame,
    const GURL& p_embedder) const {
  return (requesting_frame == p_requesting_frame) && (embedder == p_embedder);
}

bool GeolocationInfoBarQueueController::PendingInfoBarRequest::Equals(
    int p_render_process_id,
    int p_render_view_id,
    int p_bridge_id) const {
  return IsForTab(p_render_process_id, p_render_view_id) &&
     (bridge_id == p_bridge_id);
}


// GeolocationInfoBarQueueController::RequestEquals ---------------------------

// Useful predicate for checking PendingInfoBarRequest equality.
class GeolocationInfoBarQueueController::RequestEquals
    : public std::unary_function<PendingInfoBarRequest, bool> {
 public:
  RequestEquals(int render_process_id, int render_view_id, int bridge_id);

  bool operator()(const PendingInfoBarRequest& request) const;

 private:
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;
};

GeolocationInfoBarQueueController::RequestEquals::RequestEquals(
    int render_process_id,
    int render_view_id,
    int bridge_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      bridge_id_(bridge_id) {
}

bool GeolocationInfoBarQueueController::RequestEquals::operator()(
    const PendingInfoBarRequest& request) const {
  return request.Equals(render_process_id_, render_view_id_, bridge_id_);
}

// GeolocationInfoBarQueueController ------------------------------------------

GeolocationInfoBarQueueController::GeolocationInfoBarQueueController(
    NotifyPermissionSetCallback notify_permission_set_callback,
    Profile* profile)
    : notify_permission_set_callback_(notify_permission_set_callback),
      profile_(profile) {
}

GeolocationInfoBarQueueController::~GeolocationInfoBarQueueController() {
}

void GeolocationInfoBarQueueController::CreateInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We shouldn't get duplicate requests.
  DCHECK(std::find_if(pending_infobar_requests_.begin(),
      pending_infobar_requests_.end(),
      RequestEquals(render_process_id, render_view_id, bridge_id)) ==
      pending_infobar_requests_.end());

  InfoBarTabHelper* helper = GetInfoBarHelper(render_process_id,
                                              render_view_id);
  if (!helper) {
    // We won't be able to create any infobars, shortcut and remove any pending
    // requests.
    ClearPendingInfoBarRequestsForTab(render_process_id, render_view_id);
    return;
  }
  pending_infobar_requests_.push_back(PendingInfoBarRequest(render_process_id,
      render_view_id, bridge_id, requesting_frame, embedder, callback));
  if (!AlreadyShowingInfoBar(render_process_id, render_view_id))
    ShowQueuedInfoBar(render_process_id, render_view_id, helper);
}

void GeolocationInfoBarQueueController::CancelInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  PendingInfoBarRequests::iterator i = std::find_if(
      pending_infobar_requests_.begin(), pending_infobar_requests_.end(),
      RequestEquals(render_process_id, render_view_id, bridge_id));
  // TODO(pkasting): Can this conditional become a DCHECK()?
  if (i == pending_infobar_requests_.end())
    return;
  InfoBarDelegate* delegate = i->infobar_delegate;
  if (!delegate) {
    pending_infobar_requests_.erase(i);
    return;
  }
  InfoBarTabHelper* helper = GetInfoBarHelper(render_process_id,
                                              render_view_id);
  helper->RemoveInfoBar(delegate);
}

void GeolocationInfoBarQueueController::OnPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(requesting_frame.GetOrigin()),
      ContentSettingsPattern::FromURLNoWildcard(embedder.GetOrigin()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      content_setting);

  // Cancel this request first, then notify listeners.  TODO(pkasting): Why
  // is this order important?
  PendingInfoBarRequests requests_to_notify;
  PendingInfoBarRequests infobars_to_remove;
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForPair(requesting_frame, embedder)) {
      requests_to_notify.push_back(*i);
      if (i->Equals(render_process_id, render_view_id, bridge_id)) {
        // The delegate that called us is i, and it's currently in either
        // Accept() or Cancel(). This means that the owning InfoBar will call
        // RemoveInfoBar() later on, and that will trigger a notification we're
        // observing.
        ++i;
      } else if (i->infobar_delegate) {
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
       i != infobars_to_remove.end(); ++i ) {
    InfoBarTabHelper* helper = GetInfoBarHelper(i->render_process_id,
                                                i->render_view_id);
    helper->RemoveInfoBar(i->infobar_delegate);
  }

  // Send out the permission notifications.
  for (PendingInfoBarRequests::iterator i = requests_to_notify.begin();
       i != requests_to_notify.end(); ++i ) {
    notify_permission_set_callback_.Run(
        i->render_process_id, i->render_view_id,
        i->bridge_id, i->requesting_frame,
        i->callback, allowed);
  }
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
    GeolocationConfirmInfoBarDelegate* confirm_delegate = i->infobar_delegate;
    if (confirm_delegate == delegate) {
      InfoBarTabHelper* helper =
          content::Source<InfoBarTabHelper>(source).ptr();
      registrar_.Remove(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                        source);
      int render_process_id = i->render_process_id;
      int render_view_id = i->render_view_id;
      pending_infobar_requests_.erase(i);
      ShowQueuedInfoBar(render_process_id, render_view_id, helper);
      return;
    }
  }
}

void GeolocationInfoBarQueueController::ShowQueuedInfoBar(
    int render_process_id,
    int render_view_id,
    InfoBarTabHelper* helper) {
  DCHECK(helper);
  DCHECK(!AlreadyShowingInfoBar(render_process_id, render_view_id));
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->IsForTab(render_process_id, render_view_id) &&
        !i->infobar_delegate) {
      DCHECK(!registrar_.IsRegistered(
          this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
          content::Source<InfoBarTabHelper>(helper)));
      registrar_.Add(
          this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
          content::Source<InfoBarTabHelper>(helper));
      i->infobar_delegate = new GeolocationConfirmInfoBarDelegate(
          helper, this, render_process_id,
          render_view_id, i->bridge_id, i->requesting_frame,
          profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
      helper->AddInfoBar(i->infobar_delegate);
      break;
    }
  }
}

void GeolocationInfoBarQueueController::ClearPendingInfoBarRequestsForTab(
    int render_process_id,
    int render_view_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ) {
    if (i->IsForTab(render_process_id, render_view_id))
      i = pending_infobar_requests_.erase(i);
    else
      ++i;
  }
}

InfoBarTabHelper* GeolocationInfoBarQueueController::GetInfoBarHelper(
    int render_process_id,
    int render_view_id) {
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (!web_contents)
    return NULL;
  TabContents* tab_contents = TabContents::FromWebContents(web_contents);
  if (!tab_contents)
    return NULL;
  return tab_contents->infobar_tab_helper();
}

bool GeolocationInfoBarQueueController::AlreadyShowingInfoBar(
    int render_process_id,
    int render_view_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->IsForTab(render_process_id, render_view_id) && i->infobar_delegate)
      return true;
  }
  return false;
}
