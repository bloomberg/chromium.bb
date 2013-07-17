// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/two_phase_uploader.h"

#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/safe_browsing/local_two_phase_testserver.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::MessageLoopRunner;

namespace {

class Delegate {
 public:
  Delegate() : state_(TwoPhaseUploader::STATE_NONE) {
  }

  void ProgressCallback(int64 current, int64 total) {}

  void FinishCallback(scoped_refptr<MessageLoopRunner> runner,
                      TwoPhaseUploader::State state,
                      int net_error,
                      int response_code,
                      const std::string& response) {
    state_ = state;
    net_error_ = net_error;
    response_code_ = response_code;
    response_ = response;
    runner->Quit();
  }

  TwoPhaseUploader::State state_;
  int net_error_;
  int response_code_;
  std::string response_;
};

base::FilePath GetTestFilePath() {
  base::FilePath file_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path = file_path.Append(FILE_PATH_LITERAL("net"));
  file_path = file_path.Append(FILE_PATH_LITERAL("data"));
  file_path = file_path.Append(FILE_PATH_LITERAL("url_request_unittest"));
  file_path = file_path.Append(FILE_PATH_LITERAL("BullRunSpeech.txt"));
  return file_path;
}

}  // namespace

class TwoPhaseUploaderTest : public testing::Test {
 public:
  TwoPhaseUploaderTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        url_request_context_getter_(new net::TestURLRequestContextGetter(
            BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO))) {
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
};

TEST_F(TwoPhaseUploaderTest, UploadFile) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  scoped_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      url_request_context_getter_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
      test_server.GetURL("start"),
      "metadata",
      GetTestFilePath(),
      base::Bind(&Delegate::ProgressCallback, base::Unretained(&delegate)),
      base::Bind(
          &Delegate::FinishCallback, base::Unretained(&delegate), runner)));
  uploader->Start();
  runner->Run();
  EXPECT_EQ(TwoPhaseUploader::STATE_SUCCESS, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(200, delegate.response_code_);
  EXPECT_EQ(
      "/start\n"                                     // path of start request
      "4c24b2612e94e2ae622e54397663f2b7bf0a2e17\n"   // sha1sum of "metadata"
      "944857cc626f2cafe232521986b4c6d3f9993c97\n",  // sha1sum of test file
      delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, BadPhaseOneResponse) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  scoped_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      url_request_context_getter_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
      test_server.GetURL("start?p1code=500"),
      "metadata",
      GetTestFilePath(),
      base::Bind(&Delegate::ProgressCallback, base::Unretained(&delegate)),
      base::Bind(
          &Delegate::FinishCallback, base::Unretained(&delegate), runner)));
  uploader->Start();
  runner->Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_METADATA, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(500, delegate.response_code_);
  EXPECT_EQ("", delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, BadPhaseTwoResponse) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  scoped_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      url_request_context_getter_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
      test_server.GetURL("start?p2code=500"),
      "metadata",
      GetTestFilePath(),
      base::Bind(&Delegate::ProgressCallback, base::Unretained(&delegate)),
      base::Bind(
          &Delegate::FinishCallback, base::Unretained(&delegate), runner)));
  uploader->Start();
  runner->Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_FILE, delegate.state_);
  EXPECT_EQ(net::OK, delegate.net_error_);
  EXPECT_EQ(500, delegate.response_code_);
  EXPECT_EQ(
      "/start\n"                                     // path of start request
      "4c24b2612e94e2ae622e54397663f2b7bf0a2e17\n"   // sha1sum of "metadata"
      "944857cc626f2cafe232521986b4c6d3f9993c97\n",  // sha1sum of test file
      delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, PhaseOneConnectionClosed) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  scoped_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      url_request_context_getter_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
      test_server.GetURL("start?p1close=1"),
      "metadata",
      GetTestFilePath(),
      base::Bind(&Delegate::ProgressCallback, base::Unretained(&delegate)),
      base::Bind(
          &Delegate::FinishCallback, base::Unretained(&delegate), runner)));
  uploader->Start();
  runner->Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_METADATA, delegate.state_);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, delegate.net_error_);
  EXPECT_EQ(net::URLFetcher::RESPONSE_CODE_INVALID, delegate.response_code_);
  EXPECT_EQ("", delegate.response_);
}

TEST_F(TwoPhaseUploaderTest, PhaseTwoConnectionClosed) {
  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner;
  LocalTwoPhaseTestServer test_server;
  ASSERT_TRUE(test_server.Start());
  Delegate delegate;
  scoped_ptr<TwoPhaseUploader> uploader(TwoPhaseUploader::Create(
      url_request_context_getter_.get(),
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB).get(),
      test_server.GetURL("start?p2close=1"),
      "metadata",
      GetTestFilePath(),
      base::Bind(&Delegate::ProgressCallback, base::Unretained(&delegate)),
      base::Bind(
          &Delegate::FinishCallback, base::Unretained(&delegate), runner)));
  uploader->Start();
  runner->Run();
  EXPECT_EQ(TwoPhaseUploader::UPLOAD_FILE, delegate.state_);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, delegate.net_error_);
  EXPECT_EQ(net::URLFetcher::RESPONSE_CODE_INVALID, delegate.response_code_);
  EXPECT_EQ("", delegate.response_);
}
