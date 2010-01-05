// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/privacy_blacklist/blacklist_interceptor.h"
#include "chrome/browser/privacy_blacklist/blacklist_manager.h"
#include "chrome/browser/privacy_blacklist/blacklist_request_info.h"
#include "chrome/browser/privacy_blacklist/blacklist_test_util.h"
#include "chrome/common/chrome_paths.h"
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
  BlacklistInterceptorTest()
      : loop_(MessageLoop::TYPE_IO),
        ui_thread_(ChromeThread::UI, &loop_),
        file_thread_(ChromeThread::FILE, &loop_),
        io_thread_(ChromeThread::IO, &loop_) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("blacklist_samples");
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

  FilePath test_data_dir_;

 private:
  bool ContainsResourceString(const std::string& text, int string_id) {
    return text.find(l10n_util::GetStringUTF8(string_id)) != std::string::npos;
  }

  MessageLoop loop_;

  ChromeThread ui_thread_;
  ChromeThread file_thread_;
  ChromeThread io_thread_;
};

TEST_F(BlacklistInterceptorTest, Basic) {
  EXPECT_FALSE(InterceptedTestRequest(kDataUrl, NULL));
}

TEST_F(BlacklistInterceptorTest, Intercepted) {
  BlacklistTestingProfile profile;
  TestBlacklistPathProvider path_provider(&profile);
  path_provider.AddTransientPath(
      test_data_dir_.AppendASCII("annoying_ads.pbl"));
  scoped_refptr<BlacklistManager> blacklist_manager(
      new BlacklistManager(&profile, &path_provider));
  blacklist_manager->Initialize();
  MessageLoop::current()->RunAllPending();

  EXPECT_FALSE(InterceptedTestRequest(kDataUrl, NULL));

  BlacklistRequestInfo* request_info =
      new BlacklistRequestInfo(GURL(kBlockedUrl), ResourceType::MAIN_FRAME,
                               blacklist_manager.get());
  EXPECT_TRUE(InterceptedTestRequest(kBlockedUrl, request_info));
}

}  // namespace
