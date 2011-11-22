// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_IMAGE_DOWNLOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_IMAGE_DOWNLOADER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/image_decoder.h"
#include "content/public/common/url_fetcher_delegate.h"
#include "googleurl/src/gurl.h"

namespace chromeos {

// Downloads the image, decodes it in a sandboxed process.
// This objects deletes itself after OnURLFetchComplete.
class ImageDownloader : public content::URLFetcherDelegate {
 public:
  // Starts downloading the picture. Optional auth_token could be passed.
  // Object is deleted as reference counted object.
  ImageDownloader(ImageDecoder::Delegate* delegate,
                  const GURL& image_url,
                  const std::string& auth_token);
  virtual ~ImageDownloader();

 private:
  // Overriden from content::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const content::URLFetcher* source);

  ImageDecoder::Delegate* delegate_;
  scoped_ptr<content::URLFetcher> image_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ImageDownloader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_IMAGE_DOWNLOADER_H_
