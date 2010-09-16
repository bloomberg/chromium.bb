// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_model_test_util.h"

#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A Task used to coordinate when the database has finished processing
// requests. See note in BlockTillServiceProcessesRequests for details.
//
// When Run() schedules a QuitTask on the message loop it was created with.
class QuitTask2 : public Task {
 public:
  QuitTask2() : main_loop_(MessageLoop::current()) {}

  virtual void Run() {
    main_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  }

 private:
  MessageLoop* main_loop_;
};

// Blocks the caller until thread has finished servicing all pending
// requests.
static void WaitForThreadToProcessRequests(ChromeThread::ID identifier) {
  // Schedule a task on the thread that is processed after all
  // pending requests on the thread.
  ChromeThread::PostTask(identifier, FROM_HERE, new QuitTask2());
  // Run the current message loop. QuitTask2, when run, invokes Quit,
  // which unblocks this.
  MessageLoop::current()->Run();
}

}  // namespace

// Subclass the TestingProfile so that it can return a WebDataService.
class TemplateURLModelTestingProfile : public TestingProfile {
 public:
  TemplateURLModelTestingProfile() : TestingProfile() {}

  void SetUp();
  void TearDown();

  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return service_.get();
  }

 private:
  scoped_refptr<WebDataService> service_;
  ScopedTempDir temp_dir_;
  scoped_ptr<ChromeThread> db_thread_;
};

// Trivial subclass of TemplateURLModel that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLModel : public TemplateURLModel {
 public:
  explicit TestingTemplateURLModel(Profile* profile)
      : TemplateURLModel(profile) {
  }

  std::wstring GetAndClearSearchTerm() {
    std::wstring search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const std::wstring& term) {
    search_term_ = term;
  }

 private:
  std::wstring search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLModel);
};

void TemplateURLModelTestingProfile::SetUp() {
  db_thread_.reset(new ChromeThread(ChromeThread::DB));
  db_thread_->Start();

  // Make unique temp directory.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  FilePath path = temp_dir_.path().AppendASCII("TestDataService.db");
  service_ = new WebDataService;
  EXPECT_TRUE(service_->InitWithPath(path));
}

void TemplateURLModelTestingProfile::TearDown() {
  // Clean up the test directory.
  service_->Shutdown();
  // Note that we must ensure the DB thread is stopped after WDS
  // shutdown (so it can commit pending transactions) but before
  // deleting the test profile directory, otherwise we may not be
  // able to delete it due to an open transaction.
  db_thread_->Stop();
}

TemplateURLModelTestUtil::TemplateURLModelTestUtil()
    : ui_thread_(ChromeThread::UI, &message_loop_),
      changed_count_(0) {
}

TemplateURLModelTestUtil::~TemplateURLModelTestUtil() {
}

void TemplateURLModelTestUtil::SetUp() {
  profile_.reset(new TemplateURLModelTestingProfile());
  profile_->SetUp();
  model_.reset(new TestingTemplateURLModel(profile_.get()));
  model_->AddObserver(this);
}

void TemplateURLModelTestUtil::TearDown() {
  profile_->TearDown();
  TemplateURLRef::SetGoogleBaseURL(NULL);

  // Flush the message loop to make Purify happy.
  message_loop_.RunAllPending();
}

void TemplateURLModelTestUtil::OnTemplateURLModelChanged() {
  changed_count_++;
}

void TemplateURLModelTestUtil::VerifyObserverCount(int expected_changed_count) {
  ASSERT_EQ(expected_changed_count, changed_count_);
  changed_count_ = 0;
}

void TemplateURLModelTestUtil::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLModelTestUtil::BlockTillServiceProcessesRequests() {
  WaitForThreadToProcessRequests(ChromeThread::DB);
}

void TemplateURLModelTestUtil::BlockTillIOThreadProcessesRequests() {
  WaitForThreadToProcessRequests(ChromeThread::IO);
}

void TemplateURLModelTestUtil::VerifyLoad() {
  ASSERT_FALSE(model_->loaded());
  model_->Load();
  BlockTillServiceProcessesRequests();
  VerifyObserverCount(1);
}

void TemplateURLModelTestUtil::ChangeModelToLoadState() {
  model_->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.
  model_->service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
}

void TemplateURLModelTestUtil::ClearModel() {
  model_.reset(NULL);
}

void TemplateURLModelTestUtil::ResetModel(bool verify_load) {
  model_.reset(new TestingTemplateURLModel(profile_.get()));
  model_->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

std::wstring TemplateURLModelTestUtil::GetAndClearSearchTerm() {
  return model_->GetAndClearSearchTerm();
}

void TemplateURLModelTestUtil::SetGoogleBaseURL(
    const std::string& base_url) const {
  TemplateURLRef::SetGoogleBaseURL(new std::string(base_url));
  NotificationService::current()->Notify(NotificationType::GOOGLE_URL_UPDATED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

WebDataService* TemplateURLModelTestUtil::GetWebDataService() {
  return profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
}

TemplateURLModel* TemplateURLModelTestUtil::model() const {
  return model_.get();
}

TestingProfile* TemplateURLModelTestUtil::profile() const {
  return profile_.get();
}
