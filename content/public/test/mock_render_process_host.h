// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/observer_list.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/common/service_registry.h"
#include "ipc/ipc_test_sink.h"

class StoragePartition;

namespace content {

class MockRenderProcessHostFactory;

// A mock render process host that has no corresponding renderer process.  All
// IPC messages are sent into the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
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
  void EnableSendQueue() override;
  bool Init() override;
  int GetNextRoutingID() override;
  void AddRoute(int32_t routing_id, IPC::Listener* listener) override;
  void RemoveRoute(int32_t routing_id) override;
  void AddObserver(RenderProcessHostObserver* observer) override;
  void RemoveObserver(RenderProcessHostObserver* observer) override;
  void ShutdownForBadMessage() override;
  void WidgetRestored() override;
  void WidgetHidden() override;
  int VisibleWidgetCount() const override;
  void AudioStateChanged() override;
  bool IsForGuestsOnly() const override;
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
  void SetSuddenTerminationAllowed(bool allowed) override;
  bool SuddenTerminationAllowed() const override;
  BrowserContext* GetBrowserContext() const override;
  bool InSameStoragePartition(StoragePartition* partition) const override;
  IPC::ChannelProxy* GetChannel() override;
  void AddFilter(BrowserMessageFilter* filter) override;
  bool FastShutdownForPageCount(size_t count) override;
  base::TimeDelta GetChildProcessIdleTime() const override;
  void FilterURL(bool empty_allowed, GURL* url) override;
#if defined(ENABLE_WEBRTC)
  void EnableAudioDebugRecordings(const base::FilePath& file) override;
  void DisableAudioDebugRecordings() override;
  void EnableEventLogRecordings(const base::FilePath& file) override;
  void DisableEventLogRecordings() override;
  void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  void ClearWebRtcLogMessageCallback() override;
  WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  void ResumeDeferredNavigation(const GlobalRequestID& request_id) override;
  void NotifyTimezoneChange(const std::string& zone_id) override;
  ServiceRegistry* GetServiceRegistry() override;
  shell::Connection* GetChildConnection() override;
  std::unique_ptr<base::SharedPersistentMemoryAllocator> TakeMetricsAllocator()
      override;
  const base::TimeTicks& GetInitTimeForNavigationMetrics() const override;
#if defined(ENABLE_BROWSER_CDMS)
  scoped_refptr<media::MediaKeys> GetCdm(int render_frame_id,
                                         int cdm_id) const override;
#endif
  bool IsProcessBackgrounded() const override;
  void IncrementWorkerRefCount() override;
  void DecrementWorkerRefCount() override;

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

  void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const override {}

  int worker_ref_count() const { return worker_ref_count_; }

  void SetServiceRegistry(std::unique_ptr<ServiceRegistry> service_registry) {
    service_registry_ = std::move(service_registry);
  }

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;
  int id_;
  bool has_connection_;
  BrowserContext* browser_context_;
  base::ObserverList<RenderProcessHostObserver> observers_;

  IDMap<RenderWidgetHost> render_widget_hosts_;
  int prev_routing_id_;
  IDMap<IPC::Listener> listeners_;
  bool fast_shutdown_started_;
  bool deletion_callback_called_;
  bool is_for_guests_only_;
  bool is_process_backgrounded_;
  std::unique_ptr<base::ProcessHandle> process_handle;
  int worker_ref_count_;
  std::unique_ptr<ServiceRegistry> service_registry_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  ~MockRenderProcessHostFactory() override;

  RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) const override;

  // Removes the given MockRenderProcessHost from the MockRenderProcessHost list
  // without deleting it. When a test deletes a MockRenderProcessHost, we need
  // to remove it from |processes_| to prevent it from being deleted twice.
  void Remove(MockRenderProcessHost* host) const;

 private:
  // A list of MockRenderProcessHosts created by this object. This list is used
  // for deleting all MockRenderProcessHosts that have not deleted by a test in
  // the destructor and prevent them from being leaked.
  mutable ScopedVector<MockRenderProcessHost> processes_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHostFactory);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
