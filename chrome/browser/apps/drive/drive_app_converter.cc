// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_converter.h"

#include <stddef.h>
#include <algorithm>
#include <set>
#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/resource_request.h"
#include "content/public/common/simple_url_loader.h"
#include "content/public/common/url_loader_factory.mojom.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::BrowserThread;

// IconFetcher downloads |icon_url| using |converter|'s profile. The icon
// url is passed from a DriveAppInfo and should follow icon url definition
// in Drive API:
//   https://developers.google.com/drive/v2/reference/apps#resource
// Each icon url represents a single image associated with a certain size.
class DriveAppConverter::IconFetcher : public ImageDecoder::ImageRequest {
 public:
  IconFetcher(DriveAppConverter* converter,
              const GURL& icon_url,
              int expected_size)
      : converter_(converter),
        icon_url_(icon_url),
        expected_size_(expected_size) {}
  ~IconFetcher() override {}

  void Start() {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("launcher_drive_app_icon_fetch", R"(
          semantics {
            sender: "Google Drive App"
            description:
              "Drive allows user to add apps to handle their data such as "
              "Lucidchart etc. DriveAppConverter wraps those apps as a "
              "bookmark app. This service fetches the icon of a user's "
              "connected Drive app."
            trigger:
              "When Chrome detects that a Drive app is connected to a user's "
              "account"
            data:
              "URL of the required icon to fetch. No user information is sent."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            setting: "Unconditionally enabled on ChromeOS"
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");
    simple_loader_ = content::SimpleURLLoader::Create();
    content::ResourceRequest resource_request = content::ResourceRequest();
    resource_request.url = icon_url_;
    resource_request.load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
    content::mojom::URLLoaderFactory* loader_factory =
        content::BrowserContext::GetDefaultStoragePartition(
            converter_->profile_)
            ->GetURLLoaderFactoryForBrowserProcess();
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        resource_request, loader_factory, traffic_annotation,
        base::BindOnce(&DriveAppConverter::IconFetcher::OnCompleteCallback,
                       base::Unretained(this)));
  }

  const GURL& icon_url() const { return icon_url_; }
  const SkBitmap& icon() const { return icon_; }

 private:
  void OnCompleteCallback(std::unique_ptr<std::string> response_body) {
    if (!response_body) {
      converter_->OnIconFetchComplete(this);
      return;
    }

    // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
    // with the data when it is done.
    ImageDecoder::Start(this, *response_body);
  }

  // ImageDecoder::ImageRequest overrides:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    if (decoded_image.width() == expected_size_)
      icon_ = decoded_image;
    converter_->OnIconFetchComplete(this);
  }

  void OnDecodeImageFailed() override { converter_->OnIconFetchComplete(this); }

  DriveAppConverter* converter_;
  const GURL icon_url_;
  const int expected_size_;

  std::unique_ptr<content::SimpleURLLoader> simple_loader_;
  SkBitmap icon_;

  DISALLOW_COPY_AND_ASSIGN(IconFetcher);
};

DriveAppConverter::DriveAppConverter(Profile* profile,
                                     const drive::DriveAppInfo& drive_app_info,
                                     const FinishedCallback& finished_callback)
    : profile_(profile),
      drive_app_info_(drive_app_info),
      extension_(NULL),
      is_new_install_(false),
      finished_callback_(finished_callback) {
  DCHECK(profile_);
}

DriveAppConverter::~DriveAppConverter() {
  PostInstallCleanUp();
}

void DriveAppConverter::Start() {
  DCHECK(!IsStarted());

  if (drive_app_info_.app_name.empty() ||
      !drive_app_info_.create_url.is_valid()) {
    base::ResetAndReturn(&finished_callback_).Run(this, false);
    return;
  }

  VLOG(1) << "Start installing drive app: ID " << drive_app_info_.app_id
          << ", product ID " << drive_app_info_.product_id << ", name "
          << drive_app_info_.app_name;

  web_app_.title = base::UTF8ToUTF16(drive_app_info_.app_name);
  web_app_.app_url = drive_app_info_.create_url;

  std::set<int> pending_sizes;
  for (size_t i = 0; i < drive_app_info_.app_icons.size(); ++i) {
    const int icon_size = drive_app_info_.app_icons[i].first;
    if (pending_sizes.find(icon_size) != pending_sizes.end())
      continue;

    pending_sizes.insert(icon_size);
    const GURL& icon_url = drive_app_info_.app_icons[i].second;
    fetchers_.push_back(
        base::MakeUnique<IconFetcher>(this, icon_url, icon_size));
    fetchers_.back()->Start();
  }

  if (fetchers_.empty())
    StartInstall();
}

bool DriveAppConverter::IsStarted() const {
  return !fetchers_.empty() || crx_installer_.get();
}

bool DriveAppConverter::IsInstalling(const std::string& app_id) const {
  return crx_installer_.get() && crx_installer_->extension() &&
         crx_installer_->extension()->id() == app_id;
}

void DriveAppConverter::OnIconFetchComplete(const IconFetcher* fetcher) {
  const SkBitmap& icon = fetcher->icon();
  if (!icon.empty() && icon.width() != 0) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.url = fetcher->icon_url();
    icon_info.data = icon;
    icon_info.width = icon.width();
    icon_info.height = icon.height();
    web_app_.icons.push_back(icon_info);
  }

  fetchers_.erase(
      std::find_if(fetchers_.begin(), fetchers_.end(),
                   [fetcher](const std::unique_ptr<IconFetcher>& item) {
                     return item.get() == fetcher;
                   }));

  if (fetchers_.empty())
    StartInstall();
}

void DriveAppConverter::StartInstall() {
  DCHECK(!crx_installer_.get());
  crx_installer_ = extensions::CrxInstaller::CreateSilent(
      extensions::ExtensionSystem::Get(profile_)->extension_service());
  // The converted url app should not be syncable. Drive apps go with the user's
  // account and url apps will be created when needed. Syncing those apps could
  // hit an edge case where the synced url apps become orphans when the user has
  // corresponding chrome apps.
  crx_installer_->set_do_not_sync(true);

  extensions::InstallTracker::Get(profile_)->AddObserver(this);
  crx_installer_->InstallWebApp(web_app_);
}

void DriveAppConverter::PostInstallCleanUp() {
  if (!crx_installer_.get())
    return;

  extensions::InstallTracker::Get(profile_)->RemoveObserver(this);
  crx_installer_ = NULL;
}

void DriveAppConverter::OnFinishCrxInstall(const std::string& extension_id,
                                           bool success) {
  if (!crx_installer_->extension() ||
      crx_installer_->extension()->id() != extension_id) {
    return;
  }

  extension_ = crx_installer_->extension();
  is_new_install_ = success && !crx_installer_->current_version().IsValid();
  PostInstallCleanUp();

  base::ResetAndReturn(&finished_callback_).Run(this, success);
  // |finished_callback_| could delete this.
}
