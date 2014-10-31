// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUGGESTIONS_IMAGE_FETCHER_DELEGATE_H_
#define COMPONENTS_SUGGESTIONS_IMAGE_FETCHER_DELEGATE_H_

class GURL;
class SkBitmap;

namespace suggestions {

class ImageFetcherDelegate {
 public:
  ImageFetcherDelegate() {}

  // Called when an image was fetched. |url| represents the website for which
  // the image was fetched. |bitmap| stores image data owned by the caller, and
  // can be nullptr.
  virtual void OnImageFetched(const GURL& url, const SkBitmap* bitmap) = 0;

 protected:
  virtual ~ImageFetcherDelegate() {}

  DISALLOW_COPY_AND_ASSIGN(ImageFetcherDelegate);
};

}  // namespace suggestions

#endif  // COMPONENTS_SUGGESTIONS_IMAGE_FETCHER_DELEGATE_H_
