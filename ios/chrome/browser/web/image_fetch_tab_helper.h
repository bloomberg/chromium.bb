// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_IMAGE_FETCH_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_IMAGE_FETCH_TAB_HELPER_H_

#include <unordered_map>

#include "base/macros.h"
#include "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

// Gets the image data in binary string by calling injected JavaScript.
// Never keep a reference to this class, instead get it by
// ImageFetchTabHelper::FromWebState everytime.
class ImageFetchTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<ImageFetchTabHelper> {
 public:
  ~ImageFetchTabHelper() override;

  // Callback for GetImageData. |data| will be in binary format, or nullptr if
  // GetImageData failed.
  typedef base::OnceCallback<void(const std::string* data)> ImageDataCallback;

  // Gets image data as binary string by trying 2 methods in following order:
  //   1. Draw <img> to <canvas> and export its data;
  //   2. Download the image by XMLHttpRequest and hopefully get responsed from
  //   cache.
  // This method should only be called from UI thread, and |url| should be equal
  // to the resolved "src" attribute of <img>, otherwise the method 1 would
  // fail. |callback| will be called on UI thread.
  void GetImageData(const GURL& url, ImageDataCallback&& callback);

 private:
  friend class web::WebStateUserData<ImageFetchTabHelper>;

  explicit ImageFetchTabHelper(web::WebState* web_state);

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // Handler for messages sent back from injected JavaScript.
  bool OnImageDataReceived(const base::DictionaryValue& message,
                           const GURL& page_url,
                           bool has_user_gesture,
                           bool form_in_main_frame);

  // WebState this tab helper is attached to.
  web::WebState* web_state_ = nullptr;

  // Store callbacks for GetImageData, with url as key.
  std::unordered_map<int, ImageDataCallback> callbacks_;

  // |GetImageData| uses this counter as ID to match calls with callbacks. Each
  // call on |GetImageData| will increment |call_id_| by 1 and pass it as ID
  // when calling JavaScript. The ID will be regained in the message received in
  // |OnImageDataReceived| and used to invoke the corresponding callback.
  int call_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ImageFetchTabHelper);
};

#endif  // IOS_CHROME_BROWSER_WEB_IMAGE_FETCH_TAB_HELPER_H_
