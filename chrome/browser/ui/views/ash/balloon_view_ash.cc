// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/balloon_view_ash.h"

#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/icon_messages.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "webkit/glue/image_resource_fetcher.h"

namespace {

const int kNotificationIconImageSize = 32;

ash::WebNotificationTray* GetWebNotificationTray() {
  return ash::Shell::GetInstance()->
      status_area_widget()->web_notification_tray();
}

}  // namespace

class BalloonViewAsh::IconFetcher : public content::WebContentsObserver {
 public:
  IconFetcher(content::WebContents* web_contents,
              const std::string& notification_id,
              const GURL& icon_url)
      : content::WebContentsObserver(web_contents),
        request_id_(0),
        notification_id_(notification_id),
        icon_url_(icon_url) {
    Observe(web_contents);
    content::RenderViewHost* host = web_contents->GetRenderViewHost();
    host->Send(new IconMsg_DownloadFavicon(host->GetRoutingID(),
                                           ++request_id_,
                                           icon_url,
                                           kNotificationIconImageSize));
  }

  // content::WebContentsObserver override.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    bool message_handled = false;   // Allow other handlers to receive these.
    IPC_BEGIN_MESSAGE_MAP(IconFetcher, message)
      IPC_MESSAGE_HANDLER(IconHostMsg_DidDownloadFavicon, OnDidDownloadFavicon)
      IPC_MESSAGE_UNHANDLED(message_handled = false)
    IPC_END_MESSAGE_MAP()
    return message_handled;
  }

  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            const SkBitmap& bitmap) {
    if (image_url != icon_url_ || id != request_id_)
      return;
    GetWebNotificationTray()->SetNotificationImage(
        notification_id_, gfx::ImageSkia(bitmap));
  }

 private:
  int request_id_;
  std::string notification_id_;
  GURL icon_url_;

  DISALLOW_COPY_AND_ASSIGN(IconFetcher);
};

BalloonViewAsh::BalloonViewAsh(BalloonCollection* collection)
    : collection_(collection),
      balloon_(NULL) {
}

BalloonViewAsh::~BalloonViewAsh() {
}

// BalloonView interface.
void BalloonViewAsh::Show(Balloon* balloon) {
  balloon_ = balloon;
  const Notification& notification = balloon_->notification();
  std::string extension_id = GetExtensionId(balloon);
  GetWebNotificationTray()->AddNotification(notification.notification_id(),
                                            notification.title(),
                                            notification.body(),
                                            notification.display_source(),
                                            extension_id);
  FetchIcon(notification);
}

void BalloonViewAsh::Update() {
  const Notification& notification = balloon_->notification();
  GetWebNotificationTray()->UpdateNotification(notification.notification_id(),
                                               notification.title(),
                                               notification.body());
  FetchIcon(notification);
}

void BalloonViewAsh::RepositionToBalloon() {
}

void BalloonViewAsh::Close(bool by_user) {
  Notification notification(balloon_->notification());  // Copy notification
  collection_->OnBalloonClosed(balloon_);  // Deletes balloon.
  notification.Close(by_user);
  GetWebNotificationTray()->RemoveNotification(notification.notification_id());
}

gfx::Size BalloonViewAsh::GetSize() const {
  return gfx::Size();
}

BalloonHost* BalloonViewAsh::GetHost() const {
  return NULL;
}

void BalloonViewAsh::FetchIcon(const Notification& notification) {
  if (!notification.icon().empty()) {
    ash::Shell::GetInstance()->status_area_widget()->
        web_notification_tray()->SetNotificationImage(
            notification.notification_id(), notification.icon());
    return;
  }
  if (notification.icon_url().is_empty())
    return;
  content::RenderViewHost* rvh = notification.GetRenderViewHost();
  if (!rvh) {
    LOG(WARNING) << "Notification has icon url but no RenderViewHost";
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (!web_contents) {
    LOG(WARNING) << "Notification has icon url but no WebContents";
    return;
  }
  icon_fetcher_.reset(new IconFetcher(web_contents,
                                      notification.notification_id(),
                                      notification.icon_url()));
}

std::string BalloonViewAsh::GetExtensionId(Balloon* balloon) {
  ExtensionService* extension_service =
      balloon_->profile()->GetExtensionService();
  const GURL& origin = balloon_->notification().origin_url();
  const extensions::Extension* extension =
      extension_service->extensions()->GetExtensionOrAppByURL(
          ExtensionURLInfo(origin));
  if (extension)
    return extension->id();
  return std::string();
}
