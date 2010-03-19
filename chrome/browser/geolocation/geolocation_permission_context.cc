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

const FilePath::CharType kGeolocationPermissionPath[] =
    FILE_PATH_LITERAL("Geolocation");

const wchar_t kAllowedDictionaryKey[] = L"allowed";

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
  virtual void InfoBarClosed() { delete this; }
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

GeolocationPermissionContext::GeolocationPermissionContext(
    Profile* profile)
    : profile_(profile) {
}

GeolocationPermissionContext::~GeolocationPermissionContext() {
}

void GeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  RequestPermissionFromUI(render_process_id, render_view_id, bridge_id,
                          requesting_frame);
}

void GeolocationPermissionContext::SetPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, bool allowed) {
  NotifyPermissionSet(render_process_id, render_view_id, bridge_id, allowed);
}

void GeolocationPermissionContext::RequestPermissionFromUI(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &GeolocationPermissionContext::RequestPermissionFromUI,
            render_process_id, render_view_id, bridge_id, requesting_frame));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  TabContents* tab_contents =
      tab_util::GetTabContentsByID(render_process_id, render_view_id);
  if (!tab_contents) {
    // The tab may have gone away, or the request may not be from a tab at all.
    LOG(WARNING) << "Attempt to use geolocaiton tabless renderer: "
        << render_process_id << "," << render_view_id << "," << bridge_id
        << " (geolocation is not supported in extensions)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id, false);
    return;
  }
  tab_contents->AddInfoBar(new GeolocationConfirmInfoBarDelegate(tab_contents,
      this, render_process_id, render_view_id, bridge_id, requesting_frame));
}

void GeolocationPermissionContext::NotifyPermissionSet(
    int render_process_id, int render_view_id, int bridge_id, bool allowed) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this,
            &GeolocationPermissionContext::NotifyPermissionSet,
            render_process_id, render_view_id, bridge_id, allowed));
    return;
  }

  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  CallRenderViewHost(
      render_process_id, render_view_id,
      &RenderViewHost::Send,
      new ViewMsg_Geolocation_PermissionSet(render_view_id, bridge_id,
          allowed));
}
