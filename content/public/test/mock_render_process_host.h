// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_PUBLIC_TEST_MOCK_RENDER_PROCESS_HOST_H_

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_factory.h"
#include "ipc/ipc_test_sink.h"

class StoragePartition;

namespace content {

class MockRenderProcessHostFactory;

// A mock render process host that has no corresponding renderer process.  All
// IPC messages are sent into the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
  explicit MockRenderProcessHost(BrowserContext* browser_context);
  virtual ~MockRenderProcessHost();

  // Provides access to all IPC messages that would have been sent to the
  // renderer via this RenderProcessHost.
  IPC::TestSink& sink() { return sink_; }

  // Provides test access to how many times a bad message has been received.
  int bad_msg_count() const { return bad_msg_count_; }

  // RenderProcessHost implementation (public portion).
  virtual void EnableSendQueue() override;
  virtual bool Init() override;
  virtual int GetNextRoutingID() override;
  virtual void AddRoute(int32 routing_id, IPC::Listener* listener) override;
  virtual void RemoveRoute(int32 routing_id) override;
  virtual void AddObserver(RenderProcessHostObserver* observer) override;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) override;
  virtual void ReceivedBadMessage() override;
  virtual void WidgetRestored() override;
  virtual void WidgetHidden() override;
  virtual int VisibleWidgetCount() const override;
  virtual bool IsIsolatedGuest() const override;
  virtual StoragePartition* GetStoragePartition() const override;
  virtual void AddWord(const base::string16& word);
  virtual bool FastShutdownIfPossible() override;
  virtual bool FastShutdownStarted() const override;
  virtual void DumpHandles() override;
  virtual base::ProcessHandle GetHandle() const override;
  virtual int GetID() const override;
  virtual bool HasConnection() const override;
  virtual void SetIgnoreInputEvents(bool ignore_input_events) override;
  virtual bool IgnoreInputEvents() const override;
  virtual void Cleanup() override;
  virtual void AddPendingView() override;
  virtual void RemovePendingView() override;
  virtual void SetSuddenTerminationAllowed(bool allowed) override;
  virtual bool SuddenTerminationAllowed() const override;
  virtual BrowserContext* GetBrowserContext() const override;
  virtual bool InSameStoragePartition(
      StoragePartition* partition) const override;
  virtual IPC::ChannelProxy* GetChannel() override;
  virtual void AddFilter(BrowserMessageFilter* filter) override;
  virtual bool FastShutdownForPageCount(size_t count) override;
  virtual base::TimeDelta GetChildProcessIdleTime() const override;
  virtual void ResumeRequestsForView(int route_id) override;
  virtual void FilterURL(bool empty_allowed, GURL* url) override;
#if defined(ENABLE_WEBRTC)
  virtual void EnableAecDump(const base::FilePath& file) override;
  virtual void DisableAecDump() override;
  virtual void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) override;
  virtual WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) override;
#endif
  virtual void ResumeDeferredNavigation(const GlobalRequestID& request_id)
      override;
  virtual void NotifyTimezoneChange() override;
  virtual ServiceRegistry* GetServiceRegistry() override;

  // IPC::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg) override;

  // IPC::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg) override;
  virtual void OnChannelConnected(int32 peer_pid) override;

  // Attaches the factory object so we can remove this object in its destructor
  // and prevent MockRenderProcessHostFacotry from deleting it.
  void SetFactory(const MockRenderProcessHostFactory* factory) {
    factory_ = factory;
  }

  int GetActiveViewCount();

  void set_is_isolated_guest(bool is_isolated_guest) {
    is_isolated_guest_ = is_isolated_guest;
  }

  void SetProcessHandle(scoped_ptr<base::ProcessHandle> new_handle) {
    process_handle = new_handle.Pass();
  }

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;
  int id_;
  BrowserContext* browser_context_;
  ObserverList<RenderProcessHostObserver> observers_;

  IDMap<RenderWidgetHost> render_widget_hosts_;
  int prev_routing_id_;
  IDMap<IPC::Listener> listeners_;
  bool fast_shutdown_started_;
  bool deletion_callback_called_;
  bool is_isolated_guest_;
  scoped_ptr<base::ProcessHandle> process_handle;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  virtual ~MockRenderProcessHostFactory();

  virtual RenderProcessHost* CreateRenderProcessHost(
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
