// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
#define WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "ios/web/public/web_client.h"

namespace web {

// A WebClient used for testing purposes.
class TestWebClient : public web::WebClient {
 public:
  TestWebClient();
  ~TestWebClient() override;

  // WebClient implementation.
  NSString* GetEarlyPageScript() const override;
  NSString* GetEarlyPageScript(web::WebViewType web_view_type) const override;
  bool WebViewsNeedActiveStateManager() const override;

  // Changes Early Page Script for testing purposes.
  void SetEarlyPageScript(NSString* page_script);

 private:
  base::scoped_nsobject<NSString> early_page_script_;
};

}  // namespace web

#endif // WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
