// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/drive/drive_app_converter.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::BrowserThread;

// IconFetcher downloads |icon_url| using |converter|'s profile. The icon
// url is passed from a DriveAppInfo and should follow icon url definition
// in Drive API:
//   https://developers.google.com/drive/v2/reference/apps#resource
// Each icon url represents a single image associated with a certain size.
class DriveAppConverter::IconFetcher : public net::URLFetcherDelegate,
                                       public ImageDecoder::Delegate {
 public:
  IconFetcher(DriveAppConverter* converter,
              const GURL& icon_url,
              int expected_size)
      : converter_(converter),
        icon_url_(icon_url),
        expected_size_(expected_size) {}
  virtual ~IconFetcher() {
    if (image_decoder_.get())
      image_decoder_->set_delegate(NULL);
  }

  void Start() {
    fetcher_.reset(
        net::URLFetcher::Create(icon_url_, net::URLFetcher::GET, this));
    fetcher_->SetRequestContext(converter_->profile_->GetRequestContext());
    fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES);
    fetcher_->Start();
  }

  const GURL& icon_url() const { return icon_url_; }
  const SkBitmap& icon() const { return icon_; }

 private:
  // net::URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    CHECK_EQ(fetcher_.get(), source);
    scoped_ptr<net::URLFetcher> fetcher(fetcher_.Pass());

    if (!fetcher->GetStatus().is_success() ||
        fetcher->GetResponseCode() != net::HTTP_OK) {
      converter_->OnIconFetchComplete(this);
      return;
    }

    std::string unsafe_icon_data;
    fetcher->GetResponseAsString(&unsafe_icon_data);

    image_decoder_ =
        new ImageDecoder(this, unsafe_icon_data, ImageDecoder::DEFAULT_CODEC);
    image_decoder_->Start(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI));
  }

  // ImageDecoder::Delegate overrides:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    if (decoded_image.width() == expected_size_)
      icon_ = decoded_image;
    converter_->OnIconFetchComplete(this);
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    converter_->OnIconFetchComplete(this);
  }

  DriveAppConverter* converter_;
  const GURL icon_url_;
  const int expected_size_;

  scoped_ptr<net::URLFetcher> fetcher_;
  scoped_refptr<ImageDecoder> image_decoder_;
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
    finished_callback_.Run(this, false);
    return;
  }

  web_app_.title = base::UTF8ToUTF16(drive_app_info_.app_name);
  web_app_.app_url = drive_app_info_.create_url;

  const std::set<int> allowed_sizes(extension_misc::kExtensionIconSizes,
                                    extension_misc::kExtensionIconSizes +
                                        extension_misc::kNumExtensionIconSizes);
  std::set<int> pending_sizes;
  for (size_t i = 0; i < drive_app_info_.app_icons.size(); ++i) {
    const int icon_size = drive_app_info_.app_icons[i].first;
    if (allowed_sizes.find(icon_size) == allowed_sizes.end() ||
        pending_sizes.find(icon_size) != pending_sizes.end()) {
      continue;
    }

    pending_sizes.insert(icon_size);
    const GURL& icon_url = drive_app_info_.app_icons[i].second;
    IconFetcher* fetcher = new IconFetcher(this, icon_url, icon_size);
    fetchers_.push_back(fetcher);  // Pass ownership to |fetchers|.
    fetcher->Start();
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

  fetchers_.erase(std::find(fetchers_.begin(), fetchers_.end(), fetcher));

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
  is_new_install_ = success && crx_installer_->current_version().empty();
  PostInstallCleanUp();

  finished_callback_.Run(this, success);
  // |finished_callback_| could delete this.
}
