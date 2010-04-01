// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_permission_context.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/file_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/geolocation/geolocation_content_settings_map.h"
#include "chrome/browser/geolocation/geolocation_dispatcher_host.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/render_messages.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// This is the delegate used to display the confirmation info bar.
class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegate(
      TabContents* tab_contents, GeolocationPermissionContext* context,
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame_url)
      : ConfirmInfoBarDelegate(tab_contents),
        tab_contents_(tab_contents),
        context_(context),
        render_process_id_(render_process_id),
        render_view_id_(render_view_id),
        bridge_id_(bridge_id),
        requesting_frame_url_(requesting_frame_url) {
  }

  // ConfirmInfoBarDelegate
  virtual void InfoBarClosed() {
    context_->OnInfoBarClosed(tab_contents_, render_process_id_,
                              render_view_id_, bridge_id_);
    delete this;
  }
  virtual Type GetInfoBarType() { return INFO_TYPE; }
  virtual bool Accept() { return SetPermission(true); }
  virtual bool Cancel() { return SetPermission(false); }
  virtual int GetButtons() const { return BUTTON_OK | BUTTON_CANCEL; }
  virtual std::wstring GetButtonLabel(InfoBarButton button) const {
    switch (button) {
      case BUTTON_OK:
        return l10n_util::GetString(IDS_GEOLOCATION_ALLOW_BUTTON);
      case BUTTON_CANCEL:
        return l10n_util::GetString(IDS_GEOLOCATION_DENY_BUTTON);
      default:
        // All buttons are labeled above.
        NOTREACHED() << "Bad button id " << button;
        return L"";
    }
  }
  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(
        IDS_GEOLOCATION_INFOBAR_QUESTION,
        UTF8ToWide(requesting_frame_url_.GetOrigin().spec()));
  }
  virtual SkBitmap* GetIcon() const {
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_GEOLOCATION_INFOBAR_ICON);
  }
  virtual std::wstring GetLinkText() {
    return l10n_util::GetString(IDS_LEARN_MORE);
  }
  virtual bool LinkClicked(WindowOpenDisposition disposition) {
    // Ignore the click dispostion and always open in a new top level tab.
    tab_contents_->OpenURL(
        GURL(l10n_util::GetStringUTF8(IDS_LEARN_MORE_GEOLOCATION_URL)), GURL(),
        NEW_FOREGROUND_TAB, PageTransition::LINK);
    return false;  // Do not dismiss the info bar.
  }

 private:
  bool SetPermission(bool confirm) {
    context_->SetPermission(
        render_process_id_, render_view_id_, bridge_id_, requesting_frame_url_,
        confirm);
    return true;
  }

  TabContents* tab_contents_;
  scoped_refptr<GeolocationPermissionContext> context_;
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;
  GURL requesting_frame_url_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

}  // namespace

struct GeolocationPermissionContext::PendingInfoBarRequest {
  int render_process_id;
  int render_view_id;
  int bridge_id;
  GURL requesting_frame;
  // If non-NULL, it's the current geolocation infobar for this tab.
  InfoBarDelegate* infobar_delegate;

  bool IsForTab(int p_render_process_id, int p_render_view_id) const {
    return render_process_id == p_render_process_id &&
           render_view_id == p_render_view_id;
  }

  bool Equals(int p_render_process_id,
              int p_render_view_id,
              int p_bridge_id) const {
    return IsForTab(p_render_process_id, p_render_view_id) &&
           bridge_id == p_bridge_id;
  }
};

GeolocationPermissionContext::GeolocationPermissionContext(
    Profile* profile)
    : profile_(profile) {
}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &GeolocationPermissionContext::RequestGeolocationPermission,
            render_process_id, render_view_id, bridge_id, requesting_frame));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  if (!tab_contents) {
    // The tab may have gone away, or the request may not be from a tab at all.
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
        << render_process_id << "," << render_view_id << "," << bridge_id
        << " (geolocation is not supported in extensions)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, false);
    return;
  }

  GURL embedder = tab_contents->GetURL();
  ContentSetting content_setting =
      profile_->GetGeolocationContentSettingsMap()->GetContentSetting(
          requesting_frame, embedder);
  if (content_setting == CONTENT_SETTING_BLOCK) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, false);
  } else if (content_setting == CONTENT_SETTING_ALLOW) {
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, true);
  } else { // setting == ask. Prompt the user.
    RequestPermissionFromUI(tab_contents, render_process_id, render_view_id,
                            bridge_id, requesting_frame);
  }
}

void GeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfoBar(render_process_id, render_view_id, bridge_id);
}

void GeolocationPermissionContext::SetPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, bool allowed) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  GURL embedder = tab_contents->GetURL();
  ContentSetting content_setting =
      allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  profile_->GetGeolocationContentSettingsMap()->SetContentSetting(
      requesting_frame.GetOrigin(), embedder.GetOrigin(), content_setting);
  NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                      requesting_frame, allowed);
}

GeolocationArbitrator* GeolocationPermissionContext::StartUpdatingRequested(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // Note we cannot store the arbitrator as a member as it is not thread safe.
  GeolocationArbitrator* arbitrator = GeolocationArbitrator::GetInstance();

  // WebKit will not request permsission until it has received a valid
  // location, but the google network location provider will not give a
  // valid location until the user has granted permission. So we cut the Gordian
  // Knot by reusing the the 'start updating' request to also trigger
  // a 'permission request' should the provider still be awaiting permission.
  if (!arbitrator->HasPermissionBeenGranted()) {
    RequestGeolocationPermission(render_process_id, render_view_id, bridge_id,
                                 requesting_frame);
  }
  return arbitrator;
}

void GeolocationPermissionContext::StopUpdatingRequested(
    int render_process_id, int render_view_id, int bridge_id) {
  CancelPendingInfoBar(render_process_id, render_view_id, bridge_id);
}

void GeolocationPermissionContext::OnInfoBarClosed(
    TabContents* tab_contents, int render_process_id, int render_view_id,
    int bridge_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->Equals(render_process_id, render_view_id, bridge_id)) {
      pending_infobar_requests_.erase(i);
      break;
    }
  }
  // Now process the queued infobars, if any.
  ShowQueuedInfoBar(tab_contents, render_process_id, render_view_id);
}

void GeolocationPermissionContext::RequestPermissionFromUI(
    TabContents* tab_contents, int render_process_id, int render_view_id,
    int bridge_id, const GURL& requesting_frame) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  PendingInfoBarRequest pending_infobar_request;
  pending_infobar_request.render_process_id = render_process_id;
  pending_infobar_request.render_view_id = render_view_id;
  pending_infobar_request.bridge_id = bridge_id;
  pending_infobar_request.requesting_frame = requesting_frame;
  pending_infobar_request.infobar_delegate = NULL;
  pending_infobar_requests_.push_back(pending_infobar_request);
  ShowQueuedInfoBar(tab_contents, render_process_id, render_view_id);
}

void GeolocationPermissionContext::NotifyPermissionSet(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, bool allowed) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  RenderViewHostDelegate::Resource* resource =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  // TabContents may have gone away (or not exists for extension).
  if (resource)
    resource->OnGeolocationPermissionSet(requesting_frame, allowed);

  CallRenderViewHost(
      render_process_id, render_view_id,
      &RenderViewHost::Send,
      new ViewMsg_Geolocation_PermissionSet(render_view_id, bridge_id,
          allowed));
  if (allowed) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
            &GeolocationPermissionContext::NotifyArbitratorPermissionGranted,
            requesting_frame));
  }
}

void GeolocationPermissionContext::NotifyArbitratorPermissionGranted(
    const GURL& requesting_frame) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  GeolocationArbitrator::GetInstance()->OnPermissionGranted(requesting_frame);
}

void GeolocationPermissionContext::ShowQueuedInfoBar(
    TabContents* tab_contents, int render_process_id, int render_view_id) {
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->IsForTab(render_process_id, render_view_id)) {
      // Check if already displayed.
      if (i->infobar_delegate)
        break;
      i->infobar_delegate =
          new GeolocationConfirmInfoBarDelegate(
              tab_contents, this, render_process_id, render_view_id,
              i->bridge_id, i->requesting_frame);
      tab_contents->AddInfoBar(i->infobar_delegate);
      break;
    }
  }
}

void GeolocationPermissionContext::CancelPendingInfoBar(
    int render_process_id, int render_view_id, int bridge_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &GeolocationPermissionContext::CancelPendingInfoBar,
            render_process_id, render_view_id, bridge_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  for (PendingInfoBarRequests::iterator i = pending_infobar_requests_.begin();
       i != pending_infobar_requests_.end(); ++i) {
    if (i->Equals(render_process_id, render_view_id, bridge_id)) {
      TabContents* tab_contents =
          tab_util::GetTabContentsByID(render_process_id, render_view_id);
      if (tab_contents && i->infobar_delegate) {
        // Removing an infobar will remove it from the pending vector.
        tab_contents->RemoveInfoBar(i->infobar_delegate);
      } else {
        // Remove it directly from the pending vector.
        pending_infobar_requests_.erase(i);
      }
      break;
    }
  }
}
