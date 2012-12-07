// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/common/cancelable_task_tracker.h"

class Profile;
class WebIntentPickerModel;

namespace net {
  class URLFetcher;
}

namespace web_intents {

class IconLoader {
 public:
  IconLoader(Profile* profile, WebIntentPickerModel* model);
  ~IconLoader();

  // Load the favicon associated with |url|.
  void LoadFavicon(const GURL& url);

  // Load an extension icon
  void LoadExtensionIcon(const GURL& url, const std::string& extension_id);

 private:
  // Called when a favicon is returned from the FaviconService.
  void OnFaviconDataAvailable(const GURL& url,
                              const history::FaviconImageResult& image_result);

  // Called when a suggested extension's icon is fetched.
  void OnExtensionIconURLFetchComplete(const std::string& extension_id,
                                       const net::URLFetcher* source);

  // Called when an extension's icon is successfully decoded and resized.
  void OnExtensionIconAvailable(const std::string& extension_id,
                                const gfx::Image& icon_image);

  // A weak pointer to the profile for the web contents.
  Profile* profile_;

  // A weak pointer to the model being updated.
  WebIntentPickerModel* model_;

  // Used to asynchronously load favicons.
  CancelableTaskTracker cancelable_task_tracker_;

  // Factory for weak pointers used in callbacks for async calls to load icon.
  base::WeakPtrFactory<IconLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(IconLoader);
};

}  // namespace web_intents;

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_ICON_LOADER_H_
