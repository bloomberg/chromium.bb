// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/privacy_blacklist/blacklist_interceptor.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kDataUrl[] = "data:text/plain,Hello World!";
const char kBlockedUrl[] = "http://example.com/annoying_ads/ad.jpg";

class BlacklistInterceptorTest : public testing::Test {
 public:
  BlacklistInterceptorTest() : loop_(MessageLoop::TYPE_IO) {
  }

 protected:
  bool InterceptedTestRequest(const std::string& url,
                              BlacklistRequestInfo* request_info) {
    TestDelegate delegate;
    TestURLRequest request(GURL(url), &delegate);
    request.SetUserData(&BlacklistRequestInfo::kURLRequestDataKey,
                        request_info);
    request.Start();
    MessageLoop::current()->Run();

    std::string response(delegate.data_received());
    return (ContainsResourceString(response, IDS_BLACKLIST_TITLE) &&
            ContainsResourceString(response, IDS_BLACKLIST_MESSAGE));
  }

  BlacklistInterceptor interceptor_;

 private:
  bool ContainsResourceString(const std::string& text, int string_id) {
    return text.find(l10n_util::GetStringUTF8(string_id)) != std::string::npos;
  }

  MessageLoop loop_;
};

TEST_F(BlacklistInterceptorTest, Basic) {
  EXPECT_FALSE(InterceptedTestRequest(kDataUrl, NULL));
}

TEST_F(BlacklistInterceptorTest, Intercepted) {
  EXPECT_FALSE(InterceptedTestRequest(kDataUrl, NULL));

  scoped_refptr<Blacklist> blacklist;
  Blacklist::Provider* provider = new Blacklist::Provider();
  blacklist->AddProvider(provider);
  Blacklist::Entry* entry =
      new Blacklist::Entry("@/annoying_ads/@", provider, false);
  entry->AddAttributes(Blacklist::kBlockAll);
  blacklist->AddEntry(entry);

  BlacklistRequestInfo* request_info =
      new BlacklistRequestInfo(GURL(kBlockedUrl), ResourceType::MAIN_FRAME,
                               blacklist.get());
  EXPECT_TRUE(InterceptedTestRequest(kBlockedUrl, request_info));
}

}  // namespace
