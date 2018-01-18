// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_AVATAR_FETCHER_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_AVATAR_FETCHER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "url/gurl.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace network {
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

class AccountAvatarFetcherDelegate {
 public:
  virtual void UpdateAvatar(const gfx::ImageSkia& image) = 0;
};

// Helper class to download an avatar. It deletes itself once the request is
// done.
class AccountAvatarFetcher : public BitmapFetcherDelegate {
 public:
  AccountAvatarFetcher(
      const GURL& url,
      const base::WeakPtr<AccountAvatarFetcherDelegate>& delegate);

  ~AccountAvatarFetcher() override;

  void Start(network::mojom::URLLoaderFactory* loader_factory);

 private:
  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

  BitmapFetcher fetcher_;
  base::WeakPtr<AccountAvatarFetcherDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(AccountAvatarFetcher);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_AVATAR_FETCHER_H_
