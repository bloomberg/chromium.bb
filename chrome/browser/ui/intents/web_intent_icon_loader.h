// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/favicon/favicon_service.h"

class Profile;
class WebIntentPickerModel;

namespace web_intents {

class IconLoader {
 public:
  IconLoader(Profile* profile, WebIntentPickerModel* model);
  ~IconLoader();

  // Load the favicon associated with |url|.
  void LoadFavicon(const GURL& url);

 private:
  // Called when a favicon is returned from the FaviconService.
  void OnFaviconDataAvailable(
      const GURL& url,
      FaviconService::Handle handle,
      const history::FaviconImageResult& image_result);

  // A weak pointer to the profile for the web contents.
  Profile* profile_;

  // A weak pointer to the model being updated.
  WebIntentPickerModel* model_;

  // Request consumer used when asynchronously loading favicons.
  CancelableRequestConsumer favicon_consumer_;

  // Factory for weak pointers used in callbacks for async calls to load icon.
  base::WeakPtrFactory<IconLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

}  // namespace web_intents;

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_
