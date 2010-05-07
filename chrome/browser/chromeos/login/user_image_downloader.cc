// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/user_image_downloader.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/url_fetcher.h"
#include "gfx/codec/png_codec.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

namespace {

// Returns a path to a file with user image. Image will be saved in the file
// to be shown later on login screen.
std::string GetUserImagePath(const std::string& username) {
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath image_path = user_data_dir.AppendASCII(username + "_image");
  LOG(INFO) << "UserImageDownloader: image path for " << username
            << " is " << image_path.value();
  return image_path.value();
}

}  // namespace

UserImageDownloader::UserImageDownloader(const std::string& username)
    : username_(username) {
  LOG(INFO) << "UserImageDownloader: "
            << "sending request to google.com/profiles/me";
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  profile_fetcher_.reset(
      new URLFetcher(GURL("http://www.google.com/profiles/me"),
                     URLFetcher::GET,
                     this));
  profile_fetcher_->set_request_context(
      ProfileManager::GetDefaultProfile()->GetRequestContext());
  profile_fetcher_->Start();
}

UserImageDownloader::~UserImageDownloader() {
}

void UserImageDownloader::OnURLFetchComplete(const URLFetcher* source,
                                             const GURL& url,
                                             const URLRequestStatus& status,
                                             int response_code,
                                             const ResponseCookies& cookies,
                                             const std::string& data) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (response_code != 200)
    return;

  LOG(INFO) << "Url is " << url.spec();
  if (source == profile_fetcher_.get()) {
    GURL image_url;
    if (!GetImageURL(url, data, &image_url)) {
      LOG(INFO) << "UserImageDownloader: didn't find image url.";
      return;
    }
    LOG(INFO) << "UserImageDownloader: sending request to "
              << image_url;
    picture_fetcher_.reset(
        new URLFetcher(GURL(image_url), URLFetcher::GET, this));
    picture_fetcher_->set_request_context(
        ProfileManager::GetDefaultProfile()->GetRequestContext());
    picture_fetcher_->Start();
  } else if (source == picture_fetcher_.get()) {
    LOG(INFO) << "UserImageDownloader: decoding the image.";
    std::vector<unsigned char> image_data(data.begin(), data.end());
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &UserImageDownloader::DecodeImageInSandbox,
            g_browser_process->resource_dispatcher_host(),
            image_data));
  }
}

void UserImageDownloader::OnDecodeImageSucceeded(
    const SkBitmap& decoded_image) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Save the image path to preferences.
  const std::string& image_path = GetUserImagePath(username_);
  chromeos::UserManager* user_manager = chromeos::UserManager::Get();
  if (user_manager)
    user_manager->SaveUserImagePath(username_, image_path);

  // Save the image to file.
  g_browser_process->file_thread()->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(
          this,
          &UserImageDownloader::SaveImageAsPNG,
          image_path,
          decoded_image));
}

void UserImageDownloader::DecodeImageInSandbox(
    ResourceDispatcherHost* rdh,
    const std::vector<unsigned char>& image_data) {
  UtilityProcessHost* utility_process_host =
      new UtilityProcessHost(rdh,
                             this,
                             ChromeThread::UI);
  utility_process_host->StartImageDecoding(image_data);
}

bool UserImageDownloader::GetImageURL(const GURL& profile_url,
                                      const std::string& profile_page,
                                      GURL* image_url) const {
  static const char kImageUrlPrefix[] = "src=\"";
  static const char kImageUrlPostfix[] = "\"";
  static const char kImageDivClass[] = "ll_photobox";

  if (!image_url) {
    DCHECK(image_url);
    return false;
  }

  // If Google Profile already exists, we'll find <div> with class
  // ll_photobox that will contain <img> with user picture.
  size_t image_path_start = profile_page.find(kImageDivClass);
  if (image_path_start != std::string::npos) {
    image_path_start = profile_page.find(kImageUrlPrefix, image_path_start);
    if (image_path_start != std::string::npos)
      image_path_start += arraysize(kImageUrlPrefix) - 1;
  }
  if (image_path_start == std::string::npos) {
    // Uncovered case, nothing to save, return.
    return false;
  }
  size_t image_path_end =
      profile_page.find(kImageUrlPostfix, image_path_start);
  if (image_path_end == std::string::npos)
    return false;
  std::string url = profile_page.substr(image_path_start,
                                        image_path_end - image_path_start);
  if (!url.empty() && url[0] == '/')
    url = "http://" + profile_url.host() + url;
  *image_url = GURL(url);
  return true;
}

void UserImageDownloader::SaveImageAsPNG(const std::string& filename,
                                         const SkBitmap& image) {
  std::vector<unsigned char> encoded_image;
  if (!gfx::PNGCodec::EncodeBGRASkBitmap(image, true, &encoded_image))
    return;
  file_util::WriteFile(FilePath(filename),
                       reinterpret_cast<char*>(&encoded_image[0]),
                       encoded_image.size());
}

}  // namespace chromeos

