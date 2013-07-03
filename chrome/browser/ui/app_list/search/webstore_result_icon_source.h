// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_ICON_SOURCE_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/image_decoder.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_source.h"
#include "url/gurl.h"

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace app_list {

// An ImageSkiaSource for the icon of web store result. It shows web store
// icon before the app icon is fetched. When the app icon is fetched
// successfully, it creates an representation using the app icon and web store
// icon as a badge.
class WebstoreResultIconSource : public gfx::ImageSkiaSource,
                                 public net::URLFetcherDelegate,
                                 public ImageDecoder::Delegate {
 public:
  typedef base::Closure IconLoadedCallback;

  WebstoreResultIconSource(const IconLoadedCallback& icon_loaded_callback,
                           net::URLRequestContextGetter* context_getter,
                           const GURL& icon_url,
                           int icon_size);
  virtual ~WebstoreResultIconSource();

 private:
  // Invoked from GetImageForScale to download the app icon when the hosting
  // ImageSkia gets painted on screen.
  void StartIconFetch();

  // Creates the result icon by putting a small web store icon on the bottom
  // right corner as a badge.
  gfx::ImageSkiaRep CreateBadgedIcon(ui::ScaleFactor scale_factor);

  // gfx::ImageSkiaSource overrides:
  virtual gfx::ImageSkiaRep GetImageForScale(
      ui::ScaleFactor scale_factor) OVERRIDE;

  // net::URLFetcherDelegate overrides:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // ImageDecoder::Delegate overrides:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE;
  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE;

  IconLoadedCallback icon_loaded_callback_;
  net::URLRequestContextGetter* context_getter_;
  const GURL icon_url_;
  const int icon_size_;

  bool icon_fetch_attempted_;
  scoped_ptr<net::URLFetcher> icon_fetcher_;

  scoped_refptr<ImageDecoder> image_decoder_;

  gfx::ImageSkia icon_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreResultIconSource);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_WEBSTORE_RESULT_ICON_SOURCE_H_
