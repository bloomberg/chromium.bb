// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "ipc/ipc_test_sink.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/network_isolation_key.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/child_process_importance.h"
#endif

namespace network {
namespace mojom {

class URLLoaderFactory;

}  // namespace mojom
}  // namespace network

namespace content {

class MockRenderProcessHostFactory;
class SiteInstance;
class StoragePartition;

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
  void SimulateRenderProcessExit(base::TerminationStatus termination_status,
                                 int exit_code);

  // RenderProcessHost implementation (public portion).
  bool Init() override;
  void EnableSendQueue() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage(CrashReportMode crash_report_mode) override;
  void UpdateClientPriority(PriorityClient* client) override;
  int VisibleClientCount() override;
  unsigned int GetFrameDepth() override;
  bool GetIntersectsViewport() override;
  bool IsForGuestsOnly() override;
  RendererAudioOutputStreamFactoryContext*
  GetRendererAudioOutputStreamFactoryContext() override;
  void OnMediaStreamAdded() override;
  void OnMediaStreamRemoved() override;
  void OnForegroundServiceWorkerAdded() override;
  void OnForegroundServiceWorkerRemoved() override;
  StoragePartition* GetStoragePartition() override;
  virtual void AddWord(const base::string16& word);
  bool Shutdown(int exit_code) override;
  bool FastShutdownIfPossible(size_t page_count,
                              bool skip_unload_handlers) override;
  bool FastShutdownStarted() override;
  const base::Process& GetProcess() override;
  bool IsReady() override;
  int GetID() override;
  bool IsInitializedAndNotDead() override;
  void SetBlocked(bool blocked) override;
  bool IsBlocked() override;
  std::unique_ptr<base::CallbackList<void(bool)>::Subscription>
  RegisterBlockStateChangedCallback(
      const base::RepeatingCallback<void(bool)>& cb) override;
  void Cleanup() override;
  void AddPendingView() override;
  void RemovePendingView() override;
  void AddPriorityClient(PriorityClient* priority_client) override;
  void RemovePriorityClient(PriorityClient* priority_client) override;
#if defined(OS_ANDROID)
  ChildProcessImportance GetEffectiveImportance() override;
  void DumpProcessStack() override;
#endif
  void SetSuddenTerminationAllowed(bool allowed) override;
  bool SuddenTerminationAllowed() override;
  BrowserContext* GetBrowserContext() override;
  bool InSameStoragePartition(StoragePartition* partition) override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  base::TimeDelta GetChildProcessIdleTime() override;
  void FilterURL(bool empty_allowed, GURL* url) override;
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
  void EnableWebRtcEventLogOutput(int lid, int output_period_ms) override;
  void DisableWebRtcEventLogOutput(int lid) override;
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) override;
  void BindReceiver(mojo::GenericPendingReceiver receiver) override;
  const service_manager::Identity& GetChildIdentity() override;
  std::unique_ptr<base::PersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() override;
  bool IsProcessBackgrounded() override;
  size_t GetKeepAliveRefCount() const;
  void IncrementKeepAliveRefCount() override;
  void DecrementKeepAliveRefCount() override;
  void DisableKeepAliveRefCount() override;
  bool IsKeepAliveRefCountDisabled() override;
  void Resume() override;
  mojom::Renderer* GetRendererInterface() override;
  void CreateURLLoaderFactory(
      const base::Optional<url::Origin>& origin,
      network::mojom::CrossOriginEmbedderPolicy embedder_policy,
      const WebPreferences* preferences,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
          header_client,
      network::mojom::URLLoaderFactoryRequest request) override;

  void SetIsNeverSuitableForReuse() override;
  bool MayReuseHost() override;
  bool IsUnused() override;
  void SetIsUsed() override;

  bool HostHasNotBeenUsed() override;
  void LockToOrigin(const IsolationContext& isolation_context,
                    const GURL& lock_url) override;
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      const url::Origin& origin) override;
  void BindFileSystemManager(
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::FileSystemManager> receiver)
      override {}
  void BindIndexedDB(blink::mojom::IDBFactoryRequest request,
                     const url::Origin& origin) override;
  void CleanupCorbExceptionForPluginUponDestruction() override;

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

  void SetProcess(base::Process&& new_process) {
    process = std::move(new_process);
  }

  void OverrideBinderForTesting(const std::string& interface_name,
                                const InterfaceBinder& binder);

  void OverrideRendererInterfaceForTesting(
      std::unique_ptr<mojo::AssociatedInterfacePtr<mojom::Renderer>>
          renderer_interface);

  void OverrideURLLoaderFactory(network::mojom::URLLoaderFactory* factory);

  bool is_renderer_locked_to_site() const {
    return is_renderer_locked_to_site_;
  }

  int foreground_service_worker_count() const {
    return foreground_service_worker_count_;
  }

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;
  int id_;
  bool has_connection_;
  BrowserContext* browser_context_;
  base::ObserverList<RenderProcessHostObserver>::Unchecked observers_;

  base::flat_set<PriorityClient*> priority_clients_;
  int prev_routing_id_;
  base::IDMap<IPC::Listener*> listeners_;
  bool fast_shutdown_started_;
  bool deletion_callback_called_;
  bool is_for_guests_only_;
  bool is_never_suitable_for_reuse_;
  bool is_process_backgrounded_;
  bool is_unused_;
  base::Process process;
  int keep_alive_ref_count_;
  int foreground_service_worker_count_;
  std::unique_ptr<mojo::AssociatedInterfacePtr<mojom::Renderer>>
      renderer_interface_;
  std::map<std::string, InterfaceBinder> binder_overrides_;
  service_manager::Identity child_identity_;
  bool is_renderer_locked_to_site_ = false;
  network::mojom::URLLoaderFactory* url_loader_factory_;
  mojo::PendingReceiver<blink::mojom::CacheStorage> cache_storage_receiver_;
  blink::mojom::IDBFactoryRequest idb_factory_request_;
  base::WeakPtrFactory<MockRenderProcessHost> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  ~MockRenderProcessHostFactory() override;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) override;

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

  // A mock URLLoaderFactory which just fails to create a loader.
  std::unique_ptr<network::mojom::URLLoaderFactory>
      default_mock_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHostFactory);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
