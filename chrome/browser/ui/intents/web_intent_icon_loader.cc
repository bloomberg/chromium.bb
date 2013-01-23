// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_icon_loader.h"

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "skia/ext/image_operations.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

namespace {

typedef base::Callback<void(const gfx::Image&)>
    ExtensionIconAvailableCallback;

// Decodes an |icon_repsonse| as delivered via URLFetcher. The response should
// be in PNG format, but is not guaranteed to be. Posts the |callback| task
// when done.
void DecodeExtensionIconAndResize(
    scoped_ptr<std::string> icon_response,
    const ExtensionIconAvailableCallback& callback) {
  SkBitmap icon_bitmap;
  if (gfx::PNGCodec::Decode(
        reinterpret_cast<const unsigned char*>(icon_response->data()),
        icon_response->length(),
        &icon_bitmap)) {
    SkBitmap resized_icon = skia::ImageOperations::Resize(
        icon_bitmap,
        skia::ImageOperations::RESIZE_BEST,
        gfx::kFaviconSize, gfx::kFaviconSize);
    gfx::Image icon_image = gfx::Image::CreateFrom1xBitmap(resized_icon);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(callback, icon_image));
  }
}

// Self-deleting trampoline that forwards A URLFetcher response to a callback.
class URLFetcherTrampoline : public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(const net::URLFetcher* source)>
      ForwardingCallback;

  explicit URLFetcherTrampoline(const ForwardingCallback& callback)
      : callback_(callback) {}
  ~URLFetcherTrampoline() {}

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  // Fowarding callback from |OnURLFetchComplete|.
  ForwardingCallback callback_;
};

void URLFetcherTrampoline::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(!callback_.is_null());
  callback_.Run(source);
  delete source;
  delete this;
}

}

namespace web_intents {

IconLoader::IconLoader(Profile* profile,
                       WebIntentPickerModel* model)
    : profile_(profile),
      model_(model),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

IconLoader::~IconLoader() {}

void IconLoader::LoadFavicon(const GURL& url) {
  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           Profile::EXPLICIT_ACCESS);

  favicon_service->GetFaviconImageForURL(
      FaviconService::FaviconForURLParams(
          profile_, url, history::FAVICON, gfx::kFaviconSize),
      base::Bind(
          &IconLoader::OnFaviconDataAvailable,
          weak_ptr_factory_.GetWeakPtr(), url),
      &cancelable_task_tracker_);
}

void IconLoader::LoadExtensionIcon(const GURL& url,
                                   const std::string& extension_id) {

  net::URLFetcher* icon_url_fetcher = net::URLFetcher::Create(
      0,
      url,
      net::URLFetcher::GET,
      new URLFetcherTrampoline(
          base::Bind(
              &IconLoader::OnExtensionIconURLFetchComplete,
              weak_ptr_factory_.GetWeakPtr(), extension_id)));

  icon_url_fetcher->SetLoadFlags(
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES);
  icon_url_fetcher->SetRequestContext(profile_->GetRequestContext());
  icon_url_fetcher->Start();
}

void IconLoader::OnFaviconDataAvailable(
    const GURL& url,
    const history::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty())
    model_->UpdateFaviconForServiceWithURL(url, image_result.image);
}

void IconLoader::OnExtensionIconURLFetchComplete(
    const std::string& extension_id, const net::URLFetcher* source) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (source->GetResponseCode() != 200)
     return;

  scoped_ptr<std::string> response(new std::string);
  if (!source->GetResponseAsString(response.get()))
    return;

  // Decoding the extension icon needs to happen on a worker thread, not the
  // UI thread. However, callbacks to IconLoader need to happen on the UI
  // thread, since IconLoader lives on UI thread. (And WeakPtrs must be de-
  // referenced on the thread they were created on).
  ExtensionIconAvailableCallback available_callback =
      base::Bind(
          &IconLoader::OnExtensionIconAvailable,
          weak_ptr_factory_.GetWeakPtr(),
          extension_id);

  // Decode PNG and resize on worker thread.
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&DecodeExtensionIconAndResize,
                 base::Passed(&response),
                 available_callback));
}

void IconLoader::OnExtensionIconAvailable(
    const std::string& extension_id,
    const gfx::Image& icon_image) {
  model_->SetSuggestedExtensionIconWithId(extension_id, icon_image);
}

}  // namespace web_intents
