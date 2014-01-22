// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

class ServiceWorkerBrowserTest : public ContentBrowserTest,
                                 public EmbeddedWorkerInstance::Observer {
 public:
  typedef ServiceWorkerBrowserTest self;

  ServiceWorkerBrowserTest()
      : last_worker_status_(EmbeddedWorkerInstance::STOPPED) {}
  virtual ~ServiceWorkerBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableServiceWorker);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    StoragePartition* partition = BrowserContext::GetDefaultStoragePartition(
        shell()->web_contents()->GetBrowserContext());
    wrapper_ = partition->GetServiceWorkerContext();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    base::RunLoop run_loop;
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&self::TearDownOnIOThread, this, run_loop.QuitClosure()));
    run_loop.Run();
    wrapper_ = NULL;
  }

  void TearDownOnIOThread(const base::Closure& done_closure) {
    if (worker_) {
      worker_->RemoveObserver(this);
      worker_.reset();
    }
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure);
  }

  void StartEmbeddedWorkerOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    worker_ = wrapper_->context()->embedded_worker_registry()->CreateWorker();
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPED, worker_->status());
    worker_->AddObserver(this);

    // TODO(kinuko): this manual wiring should go away when this gets wired
    // in the actual code path.
    ServiceWorkerProviderHost* provider_host = GetRegisteredProviderHost();
    worker_->AddProcessReference(provider_host->process_id());

    const int64 service_worker_version_id = 33L;
    const GURL script_url = embedded_test_server()->GetURL(
        "/service_worker/worker.js");
    const bool started = worker_->Start(service_worker_version_id, script_url);

    last_worker_status_ = worker_->status();
    EXPECT_TRUE(started);
    EXPECT_EQ(EmbeddedWorkerInstance::STARTING, last_worker_status_);

    if (!started && !done_closure_.is_null())
      done_closure_.Run();
  }

  void StopEmbeddedWorkerOnIOThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::IO));
    EXPECT_EQ(EmbeddedWorkerInstance::RUNNING, worker_->status());

    const bool stopped = worker_->Stop();

    last_worker_status_ = worker_->status();
    EXPECT_TRUE(stopped);
    EXPECT_EQ(EmbeddedWorkerInstance::STOPPING, last_worker_status_);

    if (!stopped && !done_closure_.is_null())
      done_closure_.Run();
  }

 protected:
  // Embe3ddedWorkerInstance::Observer overrides:
  virtual void OnStarted() OVERRIDE {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  virtual void OnStopped() OVERRIDE {
    ASSERT_TRUE(worker_ != NULL);
    ASSERT_FALSE(done_closure_.is_null());
    last_worker_status_ = worker_->status();
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, done_closure_);
  }
  virtual void OnMessageReceived(const IPC::Message& message) OVERRIDE {
    NOTREACHED();
  }

  ServiceWorkerProviderHost* GetRegisteredProviderHost() {
    // Assumes only one provider host is registered at this point.
    std::vector<ServiceWorkerProviderHost*> providers;
    wrapper_->context()->GetAllProviderHosts(&providers);
    DCHECK_EQ(1U, providers.size());
    return providers[0];
  }

  scoped_refptr<ServiceWorkerContextWrapper> wrapper_;
  scoped_ptr<EmbeddedWorkerInstance> worker_;
  EmbeddedWorkerInstance::Status last_worker_status_;

  // Called by EmbeddedWorkerInstance::Observer overrides so that
  // test code can wait for the worker status notifications.
  base::Closure done_closure_;
};

IN_PROC_BROWSER_TEST_F(ServiceWorkerBrowserTest, EmbeddedWorkerBasic) {
  // Navigate to the page to set up a provider.
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), embedded_test_server()->GetURL("/service_worker/index.html"), 1);

  // Start a worker and wait until OnStarted() is called.
  base::RunLoop start_run_loop;
  done_closure_ = start_run_loop.QuitClosure();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&self::StartEmbeddedWorkerOnIOThread, this));
  start_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::RUNNING, last_worker_status_);

  // Stop a worker and wait until OnStopped() is called.
  base::RunLoop stop_run_loop;
  done_closure_ = stop_run_loop.QuitClosure();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&self::StopEmbeddedWorkerOnIOThread, this));
  stop_run_loop.Run();

  ASSERT_EQ(EmbeddedWorkerInstance::STOPPED, last_worker_status_);
}

}  // namespace content
