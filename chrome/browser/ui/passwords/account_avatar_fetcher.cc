// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"

#include "net/base/load_flags.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

AccountAvatarFetcher::AccountAvatarFetcher(
    const GURL& url,
    const base::WeakPtr<AccountAvatarFetcherDelegate>& delegate)
    : fetcher_(url, this), delegate_(delegate) {
}

AccountAvatarFetcher::~AccountAvatarFetcher() = default;

void AccountAvatarFetcher::Start(
    net::URLRequestContextGetter* request_context) {
  fetcher_.Init(request_context, std::string(),
                net::URLRequest::NEVER_CLEAR_REFERRER,
                net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES |
                    net::LOAD_MAYBE_USER_GESTURE);
  fetcher_.Start();
}

void AccountAvatarFetcher::OnFetchComplete(const GURL& /*url*/,
                                           const SkBitmap* bitmap) {
  if (bitmap && delegate_)
    delegate_->UpdateAvatar(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));

  delete this;
}
