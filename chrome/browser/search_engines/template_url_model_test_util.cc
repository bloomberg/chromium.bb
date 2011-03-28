// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url_model_test_util.h"

#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/test/testing_profile.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
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
static void WaitForThreadToProcessRequests(BrowserThread::ID identifier) {
  // Schedule a task on the thread that is processed after all
  // pending requests on the thread.
  BrowserThread::PostTask(identifier, FROM_HERE, new QuitTask2());
  // Run the current message loop. QuitTask2, when run, invokes Quit,
  // which unblocks this.
  MessageLoop::current()->Run();
}

}  // namespace

// Subclass the TestingProfile so that it can return a WebDataService.
class TemplateURLModelTestingProfile : public TestingProfile {
 public:
  TemplateURLModelTestingProfile()
      : TestingProfile(),
        db_thread_(BrowserThread::DB),
        io_thread_(BrowserThread::IO) {
  }

  void SetUp();
  void TearDown();

  // Starts the I/O thread. This isn't done automatically because not every test
  // needs this.
  void StartIOThread() {
    base::Thread::Options options;
    options.message_loop_type = MessageLoop::TYPE_IO;
    io_thread_.StartWithOptions(options);
  }

  virtual WebDataService* GetWebDataService(ServiceAccessType access) {
    return service_.get();
  }

 private:
  scoped_refptr<WebDataService> service_;
  ScopedTempDir temp_dir_;
  BrowserThread db_thread_;
  BrowserThread io_thread_;
};

// Trivial subclass of TemplateURLModel that records the last invocation of
// SetKeywordSearchTermsForURL.
class TestingTemplateURLModel : public TemplateURLModel {
 public:
  explicit TestingTemplateURLModel(Profile* profile)
      : TemplateURLModel(profile) {
  }

  string16 GetAndClearSearchTerm() {
    string16 search_term;
    search_term.swap(search_term_);
    return search_term;
  }

 protected:
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const string16& term) {
    search_term_ = term;
  }

 private:
  string16 search_term_;

  DISALLOW_COPY_AND_ASSIGN(TestingTemplateURLModel);
};

void TemplateURLModelTestingProfile::SetUp() {
  db_thread_.Start();

  // Make unique temp directory.
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  FilePath path = temp_dir_.path().AppendASCII("TestDataService.db");
  service_ = new WebDataService;
  ASSERT_TRUE(service_->InitWithPath(path));
}

void TemplateURLModelTestingProfile::TearDown() {
  // Clear the request context so it will get deleted. This should be done
  // before shutting down the I/O thread to avoid memory leaks.
  ResetRequestContext();

  // Wait for the delete of the request context to happen.
  if (io_thread_.IsRunning())
    TemplateURLModelTestUtil::BlockTillIOThreadProcessesRequests();

  // The I/O thread must be shutdown before the DB thread.
  io_thread_.Stop();

  // Clean up the test directory.
  if (service_.get())
    service_->Shutdown();
  // Note that we must ensure the DB thread is stopped after WDS
  // shutdown (so it can commit pending transactions) but before
  // deleting the test profile directory, otherwise we may not be
  // able to delete it due to an open transaction.
  db_thread_.Stop();
}

TemplateURLModelTestUtil::TemplateURLModelTestUtil()
    : ui_thread_(BrowserThread::UI, &message_loop_),
      changed_count_(0) {
}

TemplateURLModelTestUtil::~TemplateURLModelTestUtil() {
}

void TemplateURLModelTestUtil::SetUp() {
  profile_.reset(new TemplateURLModelTestingProfile());
  profile_->SetUp();
  profile_->SetTemplateURLModel(new TestingTemplateURLModel(profile_.get()));
  profile_->GetTemplateURLModel()->AddObserver(this);
}

void TemplateURLModelTestUtil::TearDown() {
  if (profile_.get()) {
    profile_->TearDown();
    profile_.reset();
  }
  TemplateURLRef::SetGoogleBaseURL(NULL);

  // Flush the message loop to make Purify happy.
  message_loop_.RunAllPending();
}

void TemplateURLModelTestUtil::OnTemplateURLModelChanged() {
  changed_count_++;
}

int TemplateURLModelTestUtil::GetObserverCount() {
  return changed_count_;
}

void TemplateURLModelTestUtil::ResetObserverCount() {
  changed_count_ = 0;
}

void TemplateURLModelTestUtil::BlockTillServiceProcessesRequests() {
  WaitForThreadToProcessRequests(BrowserThread::DB);
}

void TemplateURLModelTestUtil::BlockTillIOThreadProcessesRequests() {
  WaitForThreadToProcessRequests(BrowserThread::IO);
}

void TemplateURLModelTestUtil::VerifyLoad() {
  ASSERT_FALSE(model()->loaded());
  model()->Load();
  BlockTillServiceProcessesRequests();
  EXPECT_EQ(1, GetObserverCount());
  ResetObserverCount();
}

void TemplateURLModelTestUtil::ChangeModelToLoadState() {
  model()->ChangeToLoadedState();
  // Initialize the web data service so that the database gets updated with
  // any changes made.
  model()->service_ = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
}

void TemplateURLModelTestUtil::ClearModel() {
  profile_->SetTemplateURLModel(NULL);
}

void TemplateURLModelTestUtil::ResetModel(bool verify_load) {
  profile_->SetTemplateURLModel(new TestingTemplateURLModel(profile_.get()));
  model()->AddObserver(this);
  changed_count_ = 0;
  if (verify_load)
    VerifyLoad();
}

string16 TemplateURLModelTestUtil::GetAndClearSearchTerm() {
  return
      static_cast<TestingTemplateURLModel*>(model())->GetAndClearSearchTerm();
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
  return profile_->GetTemplateURLModel();
}

TestingProfile* TemplateURLModelTestUtil::profile() const {
  return profile_.get();
}

void TemplateURLModelTestUtil::StartIOThread() {
  profile_->StartIOThread();
}
