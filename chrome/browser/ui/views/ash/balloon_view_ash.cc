// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/balloon_view_ash.h"

#include "ash/shell.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
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

typedef base::Callback<void(const std::string&, const gfx::ImageSkia&)> Setter;

const std::vector<SkBitmap> kNoBitmaps;

const int kPrimaryIconImageSize = 64;
const int kSecondaryIconImageSize = 15;

// Static.
message_center::MessageCenter* GetMessageCenter() {
  return ash::Shell::GetInstance()->GetWebNotificationTray()->message_center();
}

// Static.
std::string GetExtensionId(Balloon* balloon) {
  const ExtensionURLInfo url(balloon->notification().origin_url());
  const ExtensionService* service = balloon->profile()->GetExtensionService();
  const extensions::Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(url);
  return extension ? extension->id() : std::string();
}

}  // namespace

class BalloonViewAsh::ImageDownload
    : public base::SupportsWeakPtr<ImageDownload> {
 public:
  explicit ImageDownload(const GURL& url);
  void AddCallback(int size, const Setter& callback);
  void Start(const Notification& notification);

 private:
  // FaviconHelper callback.
  virtual void Downloaded(const std::string& notification_id,
                          int download_id,
                          const GURL& image_url,
                          bool errored,
                          int requested_size,
                          const std::vector<SkBitmap>& bitmaps);

  const GURL& url_;
  int size_;
  std::vector<Setter> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ImageDownload);
};

BalloonViewAsh::ImageDownload::ImageDownload(const GURL& url)
    : url_(url),
      size_(0) {
}

// The setter callback passed in will not be called if the image download fails
// for any reason. TODO(dharcourt): Delay showing the notification until all
// images are downloaded, and return an error to the notification creator/API
// caller instead of showing a partial notification if any image download fails.
void BalloonViewAsh::ImageDownload::AddCallback(int size,
                                                const Setter& callback) {
  size_ = std::max(size_, size);
  callbacks_.push_back(callback);
}

void BalloonViewAsh::ImageDownload::Start(const Notification& notification) {
  content::RenderViewHost* host = notification.GetRenderViewHost();
  if (!host) {
    LOG(WARNING) << "Notification needs an image but has no RenderViewHost";
    return;
  }

  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(host);
  if (!contents) {
    LOG(WARNING) << "Notification needs an image but has no WebContents";
    return;
  }

  contents->DownloadFavicon(url_,
                            size_,
                            base::Bind(&ImageDownload::Downloaded,
                                       AsWeakPtr(),
                                       notification.notification_id()));
}

void BalloonViewAsh::ImageDownload::Downloaded(
    const std::string& notification_id,
    int download_id,
    const GURL& image_url,
    bool errored,
    int requested_size,
    const std::vector<SkBitmap>& bitmaps) {
  if (bitmaps.empty())
    return;
  gfx::ImageSkia image(bitmaps[0]);
  for (std::vector<Setter>::const_iterator i = callbacks_.begin();
       i != callbacks_.end();
       ++i) {
    i->Run(notification_id, image);
  }
}

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
  notification_id_ = notification.notification_id();
  GetMessageCenter()->AddNotification(notification.type(),
                                      notification_id_,
                                      notification.title(),
                                      notification.body(),
                                      notification.display_source(),
                                      GetExtensionId(balloon),
                                      notification.optional_fields());
  DownloadImages(notification);
}

void BalloonViewAsh::Update() {
  std::string previous_notification_id = notification_id_;
  const Notification& notification = balloon_->notification();
  notification_id_ = notification.notification_id();
  GetMessageCenter()->UpdateNotification(previous_notification_id,
                                         notification_id_,
                                         notification.title(),
                                         notification.body(),
                                         notification.optional_fields());
  DownloadImages(notification);
}

void BalloonViewAsh::RepositionToBalloon() {
}

void BalloonViewAsh::Close(bool by_user) {
  Notification notification(balloon_->notification());  // Copy notification.
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

void BalloonViewAsh::DownloadImages(const Notification& notification) {
  message_center::MessageCenter* center = GetMessageCenter();

  // Cancel any previous downloads.
  downloads_.clear();

  // Set the notification's primary icon, or set up a download for it.
  if (!notification.icon().isNull()) {
    center->SetNotificationPrimaryIcon(notification_id_, notification.icon());
  } else if (!notification.icon_url().is_empty()) {
    GetImageDownload(notification.icon_url()).AddCallback(
        kPrimaryIconImageSize,
        base::Bind(
            &message_center::MessageCenter::SetNotificationPrimaryIcon,
            base::Unretained(center)));
  }

  // Set up a download for the notification's secondary icon if appropriate.
  const base::DictionaryValue* optional_fields = notification.optional_fields();
  if (optional_fields &&
      optional_fields->HasKey(ui::notifications::kSecondIconUrlKey)) {
    string16 url;
    optional_fields->GetString(ui::notifications::kSecondIconUrlKey, &url);
    if (!url.empty()) {
      GetImageDownload(GURL(url)).AddCallback(
          kSecondaryIconImageSize,
          base::Bind(
              &message_center::MessageCenter::SetNotificationSecondaryIcon,
              base::Unretained(center)));
    }
  }

  // Start the downloads.
  for (ImageDownloads::const_iterator i = downloads_.begin();
       i != downloads_.end();
       ++i) {
    i->second->Start(notification);
  }
}

BalloonViewAsh::ImageDownload& BalloonViewAsh::GetImageDownload(
    const GURL& url) {
  if (!downloads_.count(url))
    downloads_[url].reset(new ImageDownload(url));
  return *downloads_[url];
}
