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
  virtual void EnableSendQueue() OVERRIDE;
  virtual bool Init() OVERRIDE;
  virtual int GetNextRoutingID() OVERRIDE;
  virtual void AddRoute(int32 routing_id, IPC::Listener* listener) OVERRIDE;
  virtual void RemoveRoute(int32 routing_id) OVERRIDE;
  virtual void AddObserver(RenderProcessHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) OVERRIDE;
  virtual void ReceivedBadMessage() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual int VisibleWidgetCount() const OVERRIDE;
  virtual bool IsIsolatedGuest() const OVERRIDE;
  virtual StoragePartition* GetStoragePartition() const OVERRIDE;
  virtual void AddWord(const base::string16& word);
  virtual bool FastShutdownIfPossible() OVERRIDE;
  virtual bool FastShutdownStarted() const OVERRIDE;
  virtual void DumpHandles() OVERRIDE;
  virtual base::ProcessHandle GetHandle() const OVERRIDE;
  virtual int GetID() const OVERRIDE;
  virtual bool HasConnection() const OVERRIDE;
  virtual void SetIgnoreInputEvents(bool ignore_input_events) OVERRIDE;
  virtual bool IgnoreInputEvents() const OVERRIDE;
  virtual void Cleanup() OVERRIDE;
  virtual void AddPendingView() OVERRIDE;
  virtual void RemovePendingView() OVERRIDE;
  virtual void SetSuddenTerminationAllowed(bool allowed) OVERRIDE;
  virtual bool SuddenTerminationAllowed() const OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual bool InSameStoragePartition(
      StoragePartition* partition) const OVERRIDE;
  virtual IPC::ChannelProxy* GetChannel() OVERRIDE;
  virtual void AddFilter(BrowserMessageFilter* filter) OVERRIDE;
  virtual bool FastShutdownForPageCount(size_t count) OVERRIDE;
  virtual base::TimeDelta GetChildProcessIdleTime() const OVERRIDE;
  virtual void ResumeRequestsForView(int route_id) OVERRIDE;
  virtual void FilterURL(bool empty_allowed, GURL* url) OVERRIDE;
#if defined(ENABLE_WEBRTC)
  virtual void EnableAecDump(const base::FilePath& file) OVERRIDE;
  virtual void DisableAecDump() OVERRIDE;
  virtual void SetWebRtcLogMessageCallback(
      base::Callback<void(const std::string&)> callback) OVERRIDE;
  virtual WebRtcStopRtpDumpCallback StartRtpDump(
      bool incoming,
      bool outgoing,
      const WebRtcRtpPacketCallback& packet_callback) OVERRIDE;
#endif
  virtual void ResumeDeferredNavigation(const GlobalRequestID& request_id)
      OVERRIDE;
  virtual void NotifyTimezoneChange() OVERRIDE;
  virtual ServiceRegistry* GetServiceRegistry() OVERRIDE;

  // IPC::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // Attaches the factory object so we can remove this object in its destructor
  // and prevent MockRenderProcessHostFacotry from deleting it.
  void SetFactory(const MockRenderProcessHostFactory* factory) {
    factory_ = factory;
  }

  int GetActiveViewCount();

  void set_is_isolated_guest(bool is_isolated_guest) {
    is_isolated_guest_ = is_isolated_guest;
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

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  virtual ~MockRenderProcessHostFactory();

  virtual RenderProcessHost* CreateRenderProcessHost(
      BrowserContext* browser_context,
      SiteInstance* site_instance) const OVERRIDE;

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
