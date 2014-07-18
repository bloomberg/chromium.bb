// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace suggestions {

// An interface to retrieve images related to a specific URL.
class ImageManager {
 public:
  ImageManager() {}
  virtual ~ImageManager() {}

  // (Re)Initializes states using data received from a SuggestionService. We're
  // not doing this in the constructor because an instance may be long-lived.
  virtual void Initialize(const SuggestionsProfile& suggestions) = 0;

  // Retrieves stored image for website |url| asynchronously. Calls |callback|
  // with Bitmap pointer if found, and NULL otherwise.
  virtual void GetImageForURL(
      const GURL& url,
      base::Callback<void(const GURL&, const SkBitmap*)> callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageManager);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_IMAGE_MANAGER_H_
