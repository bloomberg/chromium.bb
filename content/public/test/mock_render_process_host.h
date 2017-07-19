// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/observer_list.h"
#include "components/viz/service/display_embedder/shared_bitmap_allocation_notifier_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "ipc/ipc_test_sink.h"
#include "media/media_features.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider.h"

class StoragePartition;

namespace content {

class MockRenderProcessHostFactory;
class RenderWidgetHost;

// A mock render process host that has no corresponding renderer process.  All
// IPC messages are sent into the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
  using InterfaceBinder = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

  explicit MockRenderProcessHost(BrowserContext* browser_context);
  ~MockRenderProcessHost() override;

  // Provides access to all IPC messages that would have been sent to the
  // renderer via this RenderProcessHost.
  IPC::TestSink& sink() { return sink_; }

  // Provides test access to how many times a bad message has been received.
  int bad_msg_count() const { return bad_msg_count_; }

  // Provides tests a way to simulate this render process crashing.
  void SimulateCrash();

  // RenderProcessHost implementation (public portion).
  bool Init() override;
  void EnableSendQueue() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override;
  void WidgetRestored() override;
  void WidgetHidden() override;
  int VisibleWidgetCount() const override;
  bool IsForGuestsOnly() const override;
  RendererAudioOutputStreamFactoryContext*
  GetRendererAudioOutputStreamFactoryContext() override;
  void OnAudioStreamAdded() override;
  void OnAudioStreamRemoved() override;
  StoragePartition* GetStoragePartition() const override;
  virtual void AddWord(const base::string16& word);
  bool Shutdown(int exit_code, bool wait) override;
  bool FastShutdownIfPossible() override;
  bool FastShutdownStarted() const override;
  base::ProcessHandle GetHandle() const override;
  bool IsReady() const override;
  int GetID() const override;
  bool HasConnection() const override;
  void SetIgnoreInputEvents(bool ignore_input_events) override;
  bool IgnoreInputEvents() const override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddWidget(RenderWidgetHost* widget) override;
  void RemoveWidget(RenderWidgetHost* widget) override;
  void SetSuddenTerminationAllowed(bool allowed) override;
  bool SuddenTerminationAllowed() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownForPageCount(size_t count) override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void FilterURL(bool empty_allowed, GURL* url) override;
#if BUILDFLAG(ENABLE_WEBRTC)
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  bool StartWebRTCEventLog(const base::FilePath& file_path) override;
  bool StopWebRTCEventLog() override;
  void SetEchoCanceller3(bool enable) override;
  void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  void ClearWebRtcLogMessageCallback() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  void ResumeDeferredNavigation(const GlobalRequestID& request_id) override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  const service_manager::Identity& GetChildIdentity() const override;
  std::unique_ptr<base::SharedPersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
  bool IsProcessBackgrounded() const override;
  size_t GetWorkerRefCount() const override;
  void IncrementServiceWorkerRefCount() override;
  void DecrementServiceWorkerRefCount() override;
  void IncrementSharedWorkerRefCount() override;
  void DecrementSharedWorkerRefCount() override;
  void ForceReleaseWorkerRefCounts() override;
  bool IsWorkerRefCountDisabled() override;
  void PurgeAndSuspend() override;
  void Resume() override;
  mojom::Renderer* GetRendererInterface() override;
  resource_coordinator::ResourceCoordinatorInterface*
  GetProcessResourceCoordinator() override;

  void SetIsNeverSuitableForReuse() override;
  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;
  viz::SharedBitmapAllocationNotifierImpl* GetSharedBitmapAllocationNotifier()
      override;

  bool HostHasNotBeenUsed() override;

  // IPC::Sender via RenderProcessHost.
  bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnChannelConnected(int32_t peer_pid) override;

  // Attaches the factory object so we can remove this object in its destructor
  // and prevent MockRenderProcessHostFacotry from deleting it.
  void SetFactory(const MockRenderProcessHostFactory* factory) {
    factory_ = factory;
  }

  void set_is_for_guests_only(bool is_for_guests_only) {
    is_for_guests_only_ = is_for_guests_only;
  }

  void set_is_process_backgrounded(bool is_process_backgrounded) {
    is_process_backgrounded_ = is_process_backgrounded;
  }

  void SetProcessHandle(std::unique_ptr<base::ProcessHandle> new_handle) {
    process_handle = std::move(new_handle);
  }

  void OverrideBinderForTesting(const std::string& interface_name,
                                const InterfaceBinder& binder);

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;
  int id_;
  bool has_connection_;
  BrowserContext* browser_context_;
  base::ObserverList<RenderProcessHostObserver> observers_;

  int prev_routing_id_;
  IDMap<IPC::Listener*> listeners_;
  bool fast_shutdown_started_;
  bool deletion_callback_called_;
  bool is_for_guests_only_;
  bool is_process_backgrounded_;
  bool is_unused_;
  std::unique_ptr<base::ProcessHandle> process_handle;
  int worker_ref_count_;
  std::unique_ptr<mojo::AssociatedInterfacePtr<mojom::Renderer>>
      renderer_interface_;
  std::map<std::string, InterfaceBinder> binder_overrides_;
  service_manager::Identity child_identity_;
  viz::SharedBitmapAllocationNotifierImpl
      shared_bitmap_allocation_notifier_impl_;
  base::WeakPtrFactory<MockRenderProcessHost> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  ~MockRenderProcessHostFactory() override;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context) const override;

  // Removes the given MockRenderProcessHost from the MockRenderProcessHost list
  // without deleting it. When a test deletes a MockRenderProcessHost, we need
  // to remove it from |processes_| to prevent it from being deleted twice.
  void Remove(MockRenderProcessHost* host) const;

  // Retrieve the current list of mock processes.
  std::vector<std::unique_ptr<MockRenderProcessHost>>* GetProcesses() {
    return &processes_;
  }

 private:
  // A list of MockRenderProcessHosts created by this object. This list is used
  // for deleting all MockRenderProcessHosts that have not deleted by a test in
  // the destructor and prevent them from being leaked.
  mutable std::vector<std::unique_ptr<MockRenderProcessHost>> processes_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHostFactory);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
