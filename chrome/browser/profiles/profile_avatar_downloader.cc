// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_avatar_downloader.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"

namespace {
const char kHighResAvatarDownloadUrlPrefix[] =
    "https://www.gstatic.com/chrome/profile_avatars/";
}

ProfileAvatarDownloader::ProfileAvatarDownloader(
    size_t icon_index,
    const FetchCompleteCallback& callback)
    : icon_index_(icon_index),
      callback_(callback) {
  DCHECK(!callback_.is_null());
  GURL url(std::string(kHighResAvatarDownloadUrlPrefix) +
           profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index));
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("profile_avatar", R"(
        semantics {
          sender: "Profile Avatar Downloader"
          description:
            "The Chromium binary comes with a bundle of low-resolution "
            "versions of avatar images. When the user selects an avatar in "
            "chrome://settings, Chromium will download a high-resolution "
            "version from Google's static content servers for use in the "
            "people manager UI."
          trigger:
            "User selects a new avatar in chrome://settings for their profile"
          data: "None, only the filename of the png to download."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: false
          policy_exception_justification:
            "No content is being uploaded or saved; this request merely "
            "downloads a publicly available PNG file."
        })");
  fetcher_.reset(new chrome::BitmapFetcher(url, this, traffic_annotation));
}

ProfileAvatarDownloader::~ProfileAvatarDownloader() {
}

void ProfileAvatarDownloader::Start() {
  // In unit tests, the browser process can return a NULL request context.
  net::URLRequestContextGetter* request_context =
      g_browser_process->system_request_context();
  if (request_context) {
    fetcher_->Init(
        request_context,
        std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
    fetcher_->Start();
  }
}

// BitmapFetcherDelegate overrides.
void ProfileAvatarDownloader::OnFetchComplete(const GURL& url,
                                              const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  // Decode the downloaded bitmap. Ownership of the image is taken by |cache_|.
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(*bitmap);
  callback_.Run(&image,
                profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index_),
                profiles::GetPathOfHighResAvatarAtIndex(icon_index_));
}
