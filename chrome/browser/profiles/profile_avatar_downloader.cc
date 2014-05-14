// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_avatar_downloader.h"

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "net/base/load_flags.h"

namespace {
const char kHighResAvatarDownloadUrlPrefix[] =
    "http://www.gstatic.com/chrome/profile_avatars/";
}

ProfileAvatarDownloader::ProfileAvatarDownloader(
    size_t icon_index,
    const base::FilePath& profile_path,
    ProfileInfoCache* cache)
    : icon_index_(icon_index),
      profile_path_(profile_path),
      cache_(cache) {
  GURL url(std::string(kHighResAvatarDownloadUrlPrefix) +
           profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index));
  fetcher_.reset(new chrome::BitmapFetcher(url, this));
}

ProfileAvatarDownloader::~ProfileAvatarDownloader() {
}

void ProfileAvatarDownloader::Start() {
  // In unit tests, the browser process can return a NULL request context.
  net::URLRequestContextGetter* request_context =
      g_browser_process->system_request_context();
  if (request_context)
    fetcher_->Start(
        request_context,
        std::string(),
        net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
        net::LOAD_NORMAL);
}

// BitmapFetcherDelegate overrides.
void ProfileAvatarDownloader::OnFetchComplete(const GURL url,
                                              const SkBitmap* bitmap) {
  if (!bitmap || !cache_)
    return;

  // Decode the downloaded bitmap. Ownership of the image is taken by |cache_|.
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(*bitmap);
  cache_->SaveAvatarImageAtPath(&image,
      profiles::GetDefaultAvatarIconFileNameAtIndex(icon_index_),
      profiles::GetPathOfHighResAvatarAtIndex(icon_index_),
      profile_path_);
}
