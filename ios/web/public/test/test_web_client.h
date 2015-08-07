// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
#define WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_

#import <Foundation/Foundation.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ios/web/public/web_client.h"

namespace web {

// A WebClient used for testing purposes.
class TestWebClient : public web::WebClient {
 public:
  TestWebClient();
  ~TestWebClient() override;
  // WebClient implementation.
  std::string GetUserAgent(bool is_desktop_user_agent) const override;
  NSString* GetEarlyPageScript(web::WebViewType web_view_type) const override;
  bool WebViewsNeedActiveStateManager() const override;

  // Changes the user agent for testing purposes. Passing true
  // for |is_desktop_user_agent| affects the result of GetUserAgent(true) call.
  // Passing false affects the result of GetUserAgent(false).
  void SetUserAgent(const std::string& user_agent, bool is_desktop_user_agent);

  // Changes Early Page Script for testing purposes.
  void SetEarlyPageScript(NSString* page_script,
                          web::WebViewType web_view_type);

 private:
  std::string user_agent_;
  std::string desktop_user_agent_;
  base::scoped_nsobject<NSMutableDictionary> early_page_scripts_;
};

}  // namespace web

#endif // WEB_TEST_WEB_PUBLIC_TEST_TEST_WEB_CLIENT_H_
