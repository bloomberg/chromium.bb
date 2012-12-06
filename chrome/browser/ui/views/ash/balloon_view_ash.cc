// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/balloon_view_ash.h"

#include "ash/shell.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center.h"
#include "webkit/glue/image_resource_fetcher.h"

namespace {

const int kNotificationIconImageSize = 32;

message_center::MessageCenter* GetMessageCenter() {
  return ash::Shell::GetInstance()->GetWebNotificationTray()->message_center();
}

}  // namespace

BalloonViewAsh::BalloonViewAsh(BalloonCollection* collection)
    : collection_(collection),
      balloon_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

BalloonViewAsh::~BalloonViewAsh() {
}

// BalloonView interface.
void BalloonViewAsh::Show(Balloon* balloon) {
  balloon_ = balloon;
  const Notification& notification = balloon_->notification();
  current_notification_id_ = notification.notification_id();
  std::string extension_id = GetExtensionId(balloon);
  GetMessageCenter()->AddNotification(notification.type(),
                                      current_notification_id_,
                                      notification.title(),
                                      notification.body(),
                                      notification.display_source(),
                                      extension_id,
                                      notification.optional_fields());
  FetchIcon(notification);
}

void BalloonViewAsh::Update() {
  const Notification& notification = balloon_->notification();
  std::string new_notification_id = notification.notification_id();
  GetMessageCenter()->UpdateNotification(current_notification_id_,
                                         new_notification_id,
                                         notification.title(),
                                         notification.body());
  current_notification_id_ = new_notification_id;
  FetchIcon(notification);
}

void BalloonViewAsh::RepositionToBalloon() {
}

void BalloonViewAsh::Close(bool by_user) {
  Notification notification(balloon_->notification());  // Copy notification
  collection_->OnBalloonClosed(balloon_);  // Deletes balloon.
  notification.Close(by_user);
  GetMessageCenter()->RemoveNotification(notification.notification_id());
}

gfx::Size BalloonViewAsh::GetSize() const {
  return gfx::Size();
}

BalloonHost* BalloonViewAsh::GetHost() const {
  return NULL;
}

void BalloonViewAsh::DidDownloadFavicon(
    int id,
    const GURL& image_url,
    bool errored,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  if (id != current_download_id_ || bitmaps.empty())
    return;
  GetMessageCenter()->SetNotificationImage(
      cached_notification_id_, gfx::ImageSkia(bitmaps[0]));
  current_download_id_ = -1;
  cached_notification_id_.clear();
}

void BalloonViewAsh::FetchIcon(const Notification& notification) {
  if (!notification.icon().isNull()) {
    GetMessageCenter()->SetNotificationImage(
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
  current_download_id_ = web_contents->DownloadFavicon(
      notification.icon_url(), kNotificationIconImageSize,
      base::Bind(&BalloonViewAsh::DidDownloadFavicon,
                 weak_ptr_factory_.GetWeakPtr()));
  cached_notification_id_ = notification.notification_id();
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
