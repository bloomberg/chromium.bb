// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_icon_loader.h"

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "ui/gfx/favicon_size.h"

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
          profile_,
          url,
          history::FAVICON,
          gfx::kFaviconSize,
          &favicon_consumer_),
      base::Bind(
          &IconLoader::OnFaviconDataAvailable,
          weak_ptr_factory_.GetWeakPtr(),
          url));
}

void IconLoader::OnFaviconDataAvailable(
    const GURL& url,
    FaviconService::Handle,
    const history::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty())
    model_->UpdateFaviconForServiceWithURL(url, image_result.image);
}

}  // namespace web_intents
