// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/string16.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_job_worker.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "printing/printed_pages_source.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestSource : public printing::PrintedPagesSource {
 public:
  virtual string16 RenderSourceName() {
    return string16();
  }
  virtual GURL RenderSourceUrl() {
    return GURL();
  }
};

class TestPrintJobWorker : public printing::PrintJobWorker {
 public:
  explicit TestPrintJobWorker(printing::PrintJobWorkerOwner* owner)
      : printing::PrintJobWorker(owner) {
  }
  friend class TestOwner;
};

class TestOwner : public printing::PrintJobWorkerOwner {
 public:
  virtual void GetSettingsDone(const printing::PrintSettings& new_settings,
                               printing::PrintingContext::Result result) {
    EXPECT_FALSE(true);
  }
  virtual printing::PrintJobWorker* DetachWorker(
      printing::PrintJobWorkerOwner* new_owner) {
    // We're screwing up here since we're calling worker from the main thread.
    // That's fine for testing. It is actually simulating PrinterQuery behavior.
    TestPrintJobWorker* worker(new TestPrintJobWorker(new_owner));
    EXPECT_TRUE(worker->Start());
    worker->printing_context()->UseDefaultSettings();
    settings_ = worker->printing_context()->settings();
    return worker;
  }
  virtual MessageLoop* message_loop() {
    EXPECT_FALSE(true);
    return NULL;
  }
  virtual const printing::PrintSettings& settings() const {
    return settings_;
  }
  virtual int cookie() const {
    return 42;
  }
 private:
  printing::PrintSettings settings_;
};

class TestPrintJob : public printing::PrintJob {
 public:
  explicit TestPrintJob(volatile bool* check) : check_(check) {
  }
 private:
  ~TestPrintJob() {
    *check_ = true;
  }
  volatile bool* check_;
};

class TestPrintNotifObserv : public NotificationObserver {
 public:
  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    EXPECT_FALSE(true);
  }
};

}  // namespace

// Crashes, Bug 55181.
#if defined(OS_WIN)
#define MAYBE_SimplePrint DISABLED_SimplePrint
#else
#define MAYBE_SimplePrint SimplePrint
#endif
TEST(PrintJobTest, MAYBE_SimplePrint) {
  // Test the multi-threaded nature of PrintJob to make sure we can use it with
  // known lifetime.

  // This message loop is actually never run.
  MessageLoop current;

  NotificationRegistrar registrar_;
  TestPrintNotifObserv observ;
  registrar_.Add(&observ, NotificationType::ALL,
                 NotificationService::AllSources());
  volatile bool check = false;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&check));
  EXPECT_EQ(MessageLoop::current(), job->message_loop());
  scoped_refptr<TestOwner> owner(new TestOwner);
  TestSource source;
  job->Initialize(owner, &source, 1);
  job->Stop();
  job = NULL;
  EXPECT_TRUE(check);
}

TEST(PrintJobTest, SimplePrintLateInit) {
  volatile bool check = false;
  MessageLoop current;
  scoped_refptr<printing::PrintJob> job(new TestPrintJob(&check));
  job = NULL;
  EXPECT_TRUE(check);
  /* TODO(maruel): Test these.
  job->Initialize()
  job->Observe();
  job->GetSettingsDone();
  job->DetachWorker();
  job->message_loop();
  job->settings();
  job->cookie();
  job->GetSettings(printing::DEFAULTS, printing::ASK_USER, NULL);
  job->StartPrinting();
  job->Stop();
  job->Cancel();
  job->RequestMissingPages();
  job->FlushJob(timeout_ms);
  job->DisconnectSource();
  job->is_job_pending();
  job->document();
  // Private
  job->UpdatePrintedDocument(NULL);
  scoped_refptr<printing::JobEventDetails> event_details;
  job->OnNotifyPrintJobEvent(event_details);
  job->OnDocumentDone();
  job->ControlledWorkerShutdown();
  */
}
