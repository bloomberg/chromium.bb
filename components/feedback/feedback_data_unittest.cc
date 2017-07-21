// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_data.h"

#include <memory>
#include <set>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/feedback/feedback_report.h"
#include "components/feedback/feedback_uploader.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr char kHistograms[] = "";
constexpr char kImageData[] = "";
constexpr char kFileData[] = "";

class MockUploader : public feedback::FeedbackUploader, public KeyedService {
 public:
  MockUploader(content::BrowserContext* context,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : FeedbackUploader(context ? context->GetPath() : base::FilePath(),
                         task_runner) {}

  MOCK_METHOD1(DispatchReport, void(scoped_refptr<FeedbackReport>));
};

std::unique_ptr<KeyedService> CreateFeedbackUploaderService(
    content::BrowserContext* context) {
  auto uploader = base::MakeUnique<MockUploader>(
      context, FeedbackUploaderFactory::CreateUploaderTaskRunner());
  EXPECT_CALL(*uploader, DispatchReport(testing::_)).Times(1);
  return std::move(uploader);
}

std::unique_ptr<std::string> MakeScoped(const char* str) {
  return base::MakeUnique<std::string>(str);
}

}  // namespace

class FeedbackDataTest : public testing::Test {
 protected:
  FeedbackDataTest()
      : context_(new content::TestBrowserContext()),
        prefs_(new TestingPrefServiceSimple()),
        data_(new FeedbackData()) {
    user_prefs::UserPrefs::Set(context_.get(), prefs_.get());
    data_->set_context(context_.get());
    data_->set_send_report_callback(base::Bind(
        &FeedbackDataTest::set_send_report_callback, base::Unretained(this)));

    FeedbackUploaderFactory::GetInstance()->SetTestingFactory(
        context_.get(), &CreateFeedbackUploaderService);
  }

  void Send() {
    bool attached_file_completed =
        data_->attached_file_uuid().empty();
    bool screenshot_completed =
        data_->screenshot_uuid().empty();

    if (screenshot_completed && attached_file_completed) {
      data_->OnFeedbackPageDataComplete();
    }
  }

  void RunMessageLoop() {
    run_loop_.reset(new base::RunLoop());
    quit_closure_ = run_loop_->QuitClosure();
    run_loop_->Run();
  }

  void set_send_report_callback(scoped_refptr<FeedbackData> data) {
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<content::TestBrowserContext> context_;
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<FeedbackData> data_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
};

TEST_F(FeedbackDataTest, ReportSending) {
  data_->SetAndCompressHistograms(MakeScoped(kHistograms));
  data_->set_image(MakeScoped(kImageData));
  data_->AttachAndCompressFileData(MakeScoped(kFileData));
  Send();
  RunMessageLoop();
  EXPECT_TRUE(data_->IsDataComplete());
}

}  // namespace feedback
