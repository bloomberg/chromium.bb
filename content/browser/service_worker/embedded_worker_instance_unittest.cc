// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <stdint.h>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/embedded_worker.mojom.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

void SaveStatusAndCall(ServiceWorkerStatusCode* out,
                       base::OnceClosure callback,
                       ServiceWorkerStatusCode status) {
  *out = status;
  std::move(callback).Run();
}

}  // namespace

class ProviderHostEndpoints : public mojom::ServiceWorkerContainerHost {
 public:
  ProviderHostEndpoints() : binding_(this) {}

  ~ProviderHostEndpoints() override {}

  mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfoPtr() {
    DCHECK(!binding_.is_bound());
    DCHECK(!client_.is_bound());
    // Just keep the endpoints.
    mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info =
        mojom::ServiceWorkerProviderInfoForStartWorker::New();
    provider_info->registration =
        blink::mojom::ServiceWorkerRegistrationObjectInfo::New();
    provider_info->registration->options =
        blink::mojom::ServiceWorkerRegistrationOptions::New();
    binding_.Bind(mojo::MakeRequest(&provider_info->host_ptr_info));
    provider_info->client_request = mojo::MakeRequest(&client_);
    mojo::MakeRequest(&provider_info->interface_provider);
    return provider_info;
  }

 private:
  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrations(GetRegistrationsCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override {
    NOTIMPLEMENTED();
  }
  void GetControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest request) override {
    NOTIMPLEMENTED();
  }

  mojom::ServiceWorkerContainerAssociatedPtr client_;
  mojo::AssociatedBinding<mojom::ServiceWorkerContainerHost> binding_;

  DISALLOW_COPY_AND_ASSIGN(ProviderHostEndpoints);
};

class EmbeddedWorkerInstanceTest : public testing::Test,
                                   public EmbeddedWorkerInstance::Listener {
 protected:
  EmbeddedWorkerInstanceTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  enum EventType {
    PROCESS_ALLOCATED,
    START_WORKER_MESSAGE_SENT,
    STARTED,
    STOPPED,
    DETACHED,
  };

  struct EventLog {
    EventType type;
    EmbeddedWorkerStatus status;
  };

  void RecordEvent(
      EventType type,
      EmbeddedWorkerStatus status = EmbeddedWorkerStatus::STOPPED) {
    EventLog log = {type, status};
    events_.push_back(log);
  }

  void OnProcessAllocated() override { RecordEvent(PROCESS_ALLOCATED); }
  void OnStartWorkerMessageSent() override {
    RecordEvent(START_WORKER_MESSAGE_SENT);
  }
  void OnStarted() override { RecordEvent(STARTED); }
  void OnStopped(EmbeddedWorkerStatus old_status) override {
    RecordEvent(STOPPED, old_status);
  }
  void OnDetached(EmbeddedWorkerStatus old_status) override {
    RecordEvent(DETACHED, old_status);
  }

  bool OnMessageReceived(const IPC::Message&) override { return false; }

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override { helper_.reset(); }

  ServiceWorkerStatusCode StartWorker(EmbeddedWorkerInstance* worker,
                                      int id, const GURL& pattern,
                                      const GURL& url) {
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        CreateStartParams(id, pattern, url);
    worker->Start(
        std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
        CreateController(), GetInstalledScriptsInfoPtr(),
        base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    return status;
  }

  std::unique_ptr<EmbeddedWorkerStartParams>
  CreateStartParams(int version_id, const GURL& scope, const GURL& script_url) {
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        base::MakeUnique<EmbeddedWorkerStartParams>();
    params->service_worker_version_id = version_id;
    params->scope = scope;
    params->script_url = script_url;
    params->pause_after_download = false;
    params->is_installed = false;
    return params;
  }

  mojom::ServiceWorkerProviderInfoForStartWorkerPtr CreateProviderInfo(
      int /* process_id */) {
    provider_host_endpoints_.emplace_back(
        base::MakeUnique<ProviderHostEndpoints>());
    return provider_host_endpoints_.back()->CreateProviderInfoPtr();
  }

  EmbeddedWorkerInstance::ProviderInfoGetter CreateProviderInfoGetter() {
    return base::BindOnce(&EmbeddedWorkerInstanceTest::CreateProviderInfo,
                          base::Unretained(this));
  }

  mojom::ServiceWorkerEventDispatcherRequest CreateEventDispatcher() {
    dispatchers_.emplace_back();
    return mojo::MakeRequest(&dispatchers_.back());
  }

  mojom::ControllerServiceWorkerRequest CreateController() {
    controllers_.emplace_back();
    return mojo::MakeRequest(&controllers_.back());
  }

  void SetWorkerStatus(EmbeddedWorkerInstance* worker,
                       EmbeddedWorkerStatus status) {
    worker->status_ = status;
  }

  ServiceWorkerStatusCode SimulateSendStartWorker(
      EmbeddedWorkerInstance* worker,
      std::unique_ptr<EmbeddedWorkerStartParams> params) {
    return worker->SendStartWorker(std::move(params));
  }

  mojom::ServiceWorkerInstalledScriptsInfoPtr GetInstalledScriptsInfoPtr() {
    installed_scripts_managers_.emplace_back();
    auto info = mojom::ServiceWorkerInstalledScriptsInfo::New();
    info->manager_request =
        mojo::MakeRequest(&installed_scripts_managers_.back());
    return info;
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  EmbeddedWorkerRegistry* embedded_worker_registry() {
    DCHECK(context());
    return context()->embedded_worker_registry();
  }

  IPC::TestSink* ipc_sink() { return helper_->ipc_sink(); }

  std::vector<std::unique_ptr<
      EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient>>*
  mock_instance_clients() {
    return helper_->mock_instance_clients();
  }

  // Mojo endpoints.
  std::vector<mojom::ServiceWorkerEventDispatcherPtr> dispatchers_;
  std::vector<mojom::ControllerServiceWorkerPtr> controllers_;
  std::vector<mojom::ServiceWorkerInstalledScriptsManagerPtr>
      installed_scripts_managers_;
  std::vector<std::unique_ptr<ProviderHostEndpoints>> provider_host_endpoints_;

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  std::vector<EventLog> events_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerInstanceTest);
};

// A helper to simulate the start worker sequence is stalled in a worker
// process.
class StalledInStartWorkerHelper : public EmbeddedWorkerTestHelper {
 public:
  StalledInStartWorkerHelper() : EmbeddedWorkerTestHelper(base::FilePath()) {}
  ~StalledInStartWorkerHelper() override {}

  void OnStartWorker(
      int embedded_worker_id,
      int64_t service_worker_version_id,
      const GURL& scope,
      const GURL& script_url,
      bool pause_after_download,
      mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo instance_host,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info,
      mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info)
      override {
    if (force_stall_in_start_) {
      // Prepare for OnStopWorker().
      instance_host_ptr_map_[embedded_worker_id].Bind(std::move(instance_host));
      // Do nothing to simulate a stall in the worker process.
      return;
    }
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, service_worker_version_id, scope, script_url,
        pause_after_download, std::move(dispatcher_request),
        std::move(controller_request), std::move(instance_host),
        std::move(provider_info), std::move(installed_scripts_info));
  }

  void OnStopWorker(int embedded_worker_id) override {
    if (instance_host_ptr_map_[embedded_worker_id]) {
      instance_host_ptr_map_[embedded_worker_id]->OnStopped();
      base::RunLoop().RunUntilIdle();
      return;
    }
    EmbeddedWorkerTestHelper::OnStopWorker(embedded_worker_id);
  }

  void set_force_stall_in_start(bool force_stall_in_start) {
    force_stall_in_start_ = force_stall_in_start;
  }

 private:
  bool force_stall_in_start_ = true;

  std::map<
      int /* embedded_worker_id */,
      mojom::EmbeddedWorkerInstanceHostAssociatedPtr /* instance_host_ptr */>
      instance_host_ptr_map_;
};

TEST_F(EmbeddedWorkerInstanceTest, StartAndStop) {
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  worker->AddListener(this);

  const int64_t service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());

  // Start should succeed.
  ServiceWorkerStatusCode status;
  base::RunLoop run_loop;
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(service_worker_version_id, pattern, url);
  worker->Start(
      std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
      CreateController(), GetInstalledScriptsInfoPtr(),
      base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
  run_loop.Run();
  EXPECT_EQ(SERVICE_WORKER_OK, status);

  // The 'WorkerStarted' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

  // Stop the worker.
  worker->Stop();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The 'WorkerStopped' message should have been sent by
  // EmbeddedWorkerTestHelper.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Check if the IPCs are fired in expected order.
  ASSERT_EQ(4u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);
  EXPECT_EQ(STOPPED, events_[3].type);
}

// Test that a worker that failed twice will use a new render process
// on the next attempt.
TEST_F(EmbeddedWorkerInstanceTest, ForceNewProcess) {
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  const int64_t service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());

  // Also simulate adding a "newly created" process to the pattern because
  // unittests can't actually create a new process itself.
  // ServiceWorkerProcessManager only chooses this process id in unittests if
  // can_use_existing_process is false.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->new_render_process_id());

  {
    // Start once normally.
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    base::RunLoop run_loop;
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        CreateStartParams(service_worker_version_id, pattern, url);
    worker->Start(
        std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
        CreateController(), GetInstalledScriptsInfoPtr(),
        base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
    // The worker should be using the default render process.
    EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());

    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }

  // Fail twice.
  context()->UpdateVersionFailureCount(service_worker_version_id,
                                       SERVICE_WORKER_ERROR_FAILED);
  context()->UpdateVersionFailureCount(service_worker_version_id,
                                       SERVICE_WORKER_ERROR_FAILED);

  {
    // Start again.
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        CreateStartParams(service_worker_version_id, pattern, url);
    worker->Start(
        std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
        CreateController(), GetInstalledScriptsInfoPtr(),
        base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    EXPECT_EQ(EmbeddedWorkerStatus::STARTING, worker->status());
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);

    EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
    // The worker should be using the new render process.
    EXPECT_EQ(helper_->new_render_process_id(), worker->process_id());
    worker->Stop();
    base::RunLoop().RunUntilIdle();
  }
}

TEST_F(EmbeddedWorkerInstanceTest, StopWhenDevToolsAttached) {
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  const int64_t service_worker_version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Simulate adding one process to the pattern.
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());

  // Start the worker and then call StopIfIdle().
  EXPECT_EQ(SERVICE_WORKER_OK,
            StartWorker(worker.get(), service_worker_version_id, pattern, url));
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, worker->status());
  base::RunLoop().RunUntilIdle();

  // The worker must be stopped now.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Set devtools_attached to true, and do the same.
  worker->SetDevToolsAttached(true);

  EXPECT_EQ(SERVICE_WORKER_OK,
            StartWorker(worker.get(), service_worker_version_id, pattern, url));
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());
  EXPECT_EQ(helper_->mock_render_process_id(), worker->process_id());
  worker->StopIfIdle();
  base::RunLoop().RunUntilIdle();

  // The worker must not be stopped this time.
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

  // Calling Stop() actually stops the worker regardless of whether devtools
  // is attached or not.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Test that the removal of a worker from the registry doesn't remove
// other workers in the same process.
TEST_F(EmbeddedWorkerInstanceTest, RemoveWorkerInSharedProcess) {
  std::unique_ptr<EmbeddedWorkerInstance> worker1 =
      embedded_worker_registry()->CreateWorker();
  std::unique_ptr<EmbeddedWorkerInstance> worker2 =
      embedded_worker_registry()->CreateWorker();

  const int64_t version_id1 = 55L;
  const int64_t version_id2 = 56L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  int process_id = helper_->mock_render_process_id();

  helper_->SimulateAddProcessToPattern(pattern, process_id);
  {
    // Start worker1.
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        CreateStartParams(version_id1, pattern, url);
    worker1->Start(
        std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
        CreateController(), GetInstalledScriptsInfoPtr(),
        base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  {
    // Start worker2.
    ServiceWorkerStatusCode status;
    base::RunLoop run_loop;
    std::unique_ptr<EmbeddedWorkerStartParams> params =
        CreateStartParams(version_id2, pattern, url);
    worker2->Start(
        std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
        CreateController(), GetInstalledScriptsInfoPtr(),
        base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(SERVICE_WORKER_OK, status);
  }

  // The two workers share the same process.
  EXPECT_EQ(worker1->process_id(), worker2->process_id());

  // Destroy worker1. It removes itself from the registry.
  int worker1_id = worker1->embedded_worker_id();
  worker1->Stop();
  worker1.reset();

  // Only worker1 should be removed from the registry's process_map.
  EmbeddedWorkerRegistry* registry =
      helper_->context()->embedded_worker_registry();
  EXPECT_EQ(0UL, registry->worker_process_map_[process_id].count(worker1_id));
  EXPECT_EQ(1UL, registry->worker_process_map_[process_id].count(
                     worker2->embedded_worker_id()));

  worker2->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, DetachDuringProcessAllocation) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence and detach during process allocation.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, scope, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&SaveStatusAndCall, &status,
                               base::BindOnce(&base::DoNothing)));
  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by detach (see a comment on the
  // dtor of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status);
}

TEST_F(EmbeddedWorkerInstanceTest, DetachAfterSendingStartWorkerMessage) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper());
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence until a start worker message is sent.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, scope, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&SaveStatusAndCall, &status,
                               base::BindOnce(&base::DoNothing)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Detach();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by detach (see a comment on the
  // dtor of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(DETACHED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status);
}

TEST_F(EmbeddedWorkerInstanceTest, StopDuringProcessAllocation) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Stop the start worker sequence before a process is allocated.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;

  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, scope, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&SaveStatusAndCall, &status,
                               base::BindOnce(&base::DoNothing)));
  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by stop (see a comment on the dtor
  // of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "PROCESS_ALLOCATED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[0].status);
  events_.clear();

  // Restart the worker.
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);
  params = CreateStartParams(version_id, scope, url);
  worker->Start(
      std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
      CreateController(), GetInstalledScriptsInfoPtr(),
      base::BindOnce(&SaveStatusAndCall, &status, run_loop->QuitClosure()));
  run_loop->Run();

  EXPECT_EQ(SERVICE_WORKER_OK, status);
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

class DontReceiveResumeAfterDownloadInstanceClient
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit DontReceiveResumeAfterDownloadInstanceClient(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper,
      bool* was_resume_after_download_called)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper),
        was_resume_after_download_called_(was_resume_after_download_called) {}

 private:
  void ResumeAfterDownload() override {
    *was_resume_after_download_called_ = true;
  }

  bool* const was_resume_after_download_called_;
};

TEST_F(EmbeddedWorkerInstanceTest, StopDuringPausedAfterDownload) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  bool was_resume_after_download_called = false;
  helper_->RegisterMockInstanceClient(
      base::MakeUnique<DontReceiveResumeAfterDownloadInstanceClient>(
          helper_->AsWeakPtr(), &was_resume_after_download_called));

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence until pause after download.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;

  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, scope, url);
  params->pause_after_download = true;
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&SaveStatusAndCall, &status,
                               base::BindOnce(&base::DoNothing)));
  base::RunLoop().RunUntilIdle();

  // Make the worker stopping and attempt to send a resume after download
  // message.
  worker->Stop();
  worker->ResumeAfterDownload();
  base::RunLoop().RunUntilIdle();

  // The resume after download message should not have been sent.
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_FALSE(was_resume_after_download_called);
}

TEST_F(EmbeddedWorkerInstanceTest, StopAfterSendingStartWorkerMessage) {
  const int64_t version_id = 55L;
  const GURL scope("http://example.com/");
  const GURL url("http://example.com/worker.js");

  helper_.reset(new StalledInStartWorkerHelper);
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  worker->AddListener(this);

  // Run the start worker sequence until a start worker message is sent.
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, scope, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&SaveStatusAndCall, &status,
                               base::BindOnce(&base::DoNothing)));
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  events_.clear();

  worker->Stop();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, worker->process_id());

  // The start callback should not be aborted by stop (see a comment on the dtor
  // of EmbeddedWorkerInstance::StartTask).
  EXPECT_EQ(SERVICE_WORKER_ERROR_MAX_VALUE, status);

  // "STARTED" event should not be recorded.
  ASSERT_EQ(1u, events_.size());
  EXPECT_EQ(STOPPED, events_[0].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPING, events_[0].status);
  events_.clear();

  // Restart the worker.
  static_cast<StalledInStartWorkerHelper*>(helper_.get())
      ->set_force_stall_in_start(false);
  status = SERVICE_WORKER_ERROR_MAX_VALUE;
  std::unique_ptr<base::RunLoop> run_loop(new base::RunLoop);

  params = CreateStartParams(version_id, scope, url);
  worker->Start(
      std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
      CreateController(), GetInstalledScriptsInfoPtr(),
      base::BindOnce(&SaveStatusAndCall, &status, run_loop->QuitClosure()));
  run_loop->Run();

  // The worker should be started.
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);

  // Tear down the worker.
  worker->Stop();
}

TEST_F(EmbeddedWorkerInstanceTest, Detach) {
  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  worker->AddListener(this);

  // Start the worker.
  base::RunLoop run_loop;
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, pattern, url);
  worker->Start(
      std::move(params), CreateProviderInfoGetter(), CreateEventDispatcher(),
      CreateController(), GetInstalledScriptsInfoPtr(),
      base::BindOnce(&SaveStatusAndCall, &status, run_loop.QuitClosure()));
  run_loop.Run();

  // Detach.
  int process_id = worker->process_id();
  worker->Detach();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());

  // Send the registry a message from the detached worker. Nothing should
  // happen.
  embedded_worker_registry()->OnWorkerStarted(process_id,
                                              worker->embedded_worker_id());
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Test for when sending the start IPC failed.
TEST_F(EmbeddedWorkerInstanceTest, FailToSendStartIPC) {
  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; mojo IPC fails to connect to a remote interface.
  helper_->RegisterMockInstanceClient(nullptr);

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  worker->AddListener(this);

  // Attempt to start the worker.
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, pattern, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Worker should handle the failure of binding on the remote side as detach.
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[2].status);
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

class FailEmbeddedWorkerInstanceClientImpl
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit FailEmbeddedWorkerInstanceClientImpl(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper) {}

 private:
  void StartWorker(const EmbeddedWorkerStartParams&,
                   mojom::ServiceWorkerEventDispatcherRequest,
                   mojom::ControllerServiceWorkerRequest,
                   mojom::ServiceWorkerInstalledScriptsInfoPtr /* unused */,
                   mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo,
                   mojom::ServiceWorkerProviderInfoForStartWorkerPtr,
                   blink::mojom::WorkerContentSettingsProxyPtr
                       content_settings_proxy) override {
    helper_->mock_instance_clients()->clear();
  }
};

TEST_F(EmbeddedWorkerInstanceTest, RemoveRemoteInterface) {
  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");

  // Let StartWorker fail; binding is discarded in the middle of IPC
  helper_->RegisterMockInstanceClient(
      base::MakeUnique<FailEmbeddedWorkerInstanceClientImpl>(
          helper_->AsWeakPtr()));
  ASSERT_EQ(mock_instance_clients()->size(), 1UL);

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  worker->AddListener(this);

  // Attempt to start the worker.
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, pattern, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();

  // Worker should handle the sudden shutdown as detach.
  ASSERT_EQ(3u, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(DETACHED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::STARTING, events_[2].status);
}

class StoreMessageInstanceClient
    : public EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient {
 public:
  explicit StoreMessageInstanceClient(
      base::WeakPtr<EmbeddedWorkerTestHelper> helper)
      : EmbeddedWorkerTestHelper::MockEmbeddedWorkerInstanceClient(helper) {}

  const std::vector<std::pair<blink::WebConsoleMessage::Level, std::string>>&
  message() {
    return messages_;
  }

 private:
  void AddMessageToConsole(blink::WebConsoleMessage::Level level,
                           const std::string& message) override {
    messages_.push_back(std::make_pair(level, message));
  }

  std::vector<std::pair<blink::WebConsoleMessage::Level, std::string>>
      messages_;
};

TEST_F(EmbeddedWorkerInstanceTest, AddMessageToConsole) {
  const int64_t version_id = 55L;
  const GURL pattern("http://example.com/");
  const GURL url("http://example.com/worker.js");
  std::unique_ptr<StoreMessageInstanceClient> instance_client =
      base::MakeUnique<StoreMessageInstanceClient>(helper_->AsWeakPtr());
  StoreMessageInstanceClient* instance_client_rawptr = instance_client.get();
  helper_->RegisterMockInstanceClient(std::move(instance_client));
  ASSERT_EQ(mock_instance_clients()->size(), 1UL);

  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  helper_->SimulateAddProcessToPattern(pattern,
                                       helper_->mock_render_process_id());
  worker->AddListener(this);

  // Attempt to start the worker and immediate AddMessageToConsole should not
  // cause a crash.
  std::pair<blink::WebConsoleMessage::Level, std::string> test_message =
      std::make_pair(blink::WebConsoleMessage::kLevelVerbose, "");
  std::unique_ptr<EmbeddedWorkerStartParams> params =
      CreateStartParams(version_id, pattern, url);
  worker->Start(std::move(params), CreateProviderInfoGetter(),
                CreateEventDispatcher(), CreateController(),
                GetInstalledScriptsInfoPtr(),
                base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  worker->AddMessageToConsole(test_message.first, test_message.second);
  base::RunLoop().RunUntilIdle();

  // Messages sent before sending StartWorker message won't be dispatched.
  ASSERT_EQ(0UL, instance_client_rawptr->message().size());
  ASSERT_EQ(3UL, events_.size());
  EXPECT_EQ(PROCESS_ALLOCATED, events_[0].type);
  EXPECT_EQ(START_WORKER_MESSAGE_SENT, events_[1].type);
  EXPECT_EQ(STARTED, events_[2].type);
  EXPECT_EQ(EmbeddedWorkerStatus::RUNNING, worker->status());

  worker->AddMessageToConsole(test_message.first, test_message.second);
  base::RunLoop().RunUntilIdle();

  // Messages sent after sending StartWorker message should be reached to
  // the renderer.
  ASSERT_EQ(1UL, instance_client_rawptr->message().size());
  EXPECT_EQ(test_message, instance_client_rawptr->message()[0]);

  // Ensure the worker is stopped.
  worker->Stop();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(EmbeddedWorkerStatus::STOPPED, worker->status());
}

// Test that SendStartWorker checks if dispatcher host exists.
TEST_F(EmbeddedWorkerInstanceTest, NoDispatcherHost) {
  std::unique_ptr<EmbeddedWorkerInstance> worker =
      embedded_worker_registry()->CreateWorker();
  SetWorkerStatus(worker.get(), EmbeddedWorkerStatus::STARTING);
  auto params = base::MakeUnique<EmbeddedWorkerStartParams>();
  ServiceWorkerStatusCode result =
      SimulateSendStartWorker(worker.get(), std::move(params));
  EXPECT_EQ(SERVICE_WORKER_ERROR_IPC_FAILED, result);
  // Set to STOPPED because EWInstance's destructor DCHECKs status.
  SetWorkerStatus(worker.get(), EmbeddedWorkerStatus::STOPPED);
}

}  // namespace content
