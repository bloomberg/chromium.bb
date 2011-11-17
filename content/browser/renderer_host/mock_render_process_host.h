// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "ipc/ipc_test_sink.h"

class MockRenderProcessHostFactory;
class TransportDIB;

// A mock render process host that has no corresponding renderer process.  All
// IPC messages are sent into the message sink for inspection by tests.
class MockRenderProcessHost : public RenderProcessHost {
 public:
  explicit MockRenderProcessHost(content::BrowserContext* browser_context);
  virtual ~MockRenderProcessHost();

  // Provides access to all IPC messages that would have been sent to the
  // renderer via this RenderProcessHost.
  IPC::TestSink& sink() { return sink_; }

  // Provides tests access to the max page ID currently used for this process.
  int max_page_id() const { return max_page_id_; }

  // Provides test access to how many times a bad message has been received.
  int bad_msg_count() const { return bad_msg_count_; }

  // RenderProcessHost implementation (public portion).
  virtual void EnableSendQueue() OVERRIDE;
  virtual bool Init(bool is_accessibility_enabled) OVERRIDE;
  virtual int GetNextRoutingID() OVERRIDE;
  virtual void UpdateAndSendMaxPageID(int32 page_id) OVERRIDE;
  virtual void CancelResourceRequests(int render_widget_id) OVERRIDE;
  virtual void CrossSiteSwapOutACK(
      const ViewMsg_SwapOut_Params& params) OVERRIDE;
  virtual bool WaitForUpdateMsg(int render_widget_id,
                                const base::TimeDelta& max_delay,
                                IPC::Message* msg) OVERRIDE;
  virtual void ReceivedBadMessage() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual int VisibleWidgetCount() const OVERRIDE;
  virtual void AddWord(const string16& word);
  virtual bool FastShutdownIfPossible() OVERRIDE;
  virtual void DumpHandles() OVERRIDE;
  virtual base::ProcessHandle GetHandle() OVERRIDE;
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id) OVERRIDE;
  virtual void SetCompositingSurface(
      int render_widget_id,
      gfx::PluginWindowHandle compositing_surface) OVERRIDE;

  // IPC::Channel::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Channel::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  // Attaches the factory object so we can remove this object in its destructor
  // and prevent MockRenderProcessHostFacotry from deleting it.
  void SetFactory(const MockRenderProcessHostFactory* factory) {
    factory_ = factory;
  }

 private:
  // Stores IPC messages that would have been sent to the renderer.
  IPC::TestSink sink_;
  TransportDIB* transport_dib_;
  int bad_msg_count_;
  const MockRenderProcessHostFactory* factory_;

  DISALLOW_COPY_AND_ASSIGN(MockRenderProcessHost);
};

class MockRenderProcessHostFactory : public RenderProcessHostFactory {
 public:
  MockRenderProcessHostFactory();
  virtual ~MockRenderProcessHostFactory();

  virtual RenderProcessHost* CreateRenderProcessHost(
      content::BrowserContext* browser_context) const OVERRIDE;

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

#endif  // CONTENT_BROWSER_RENDERER_HOST_MOCK_RENDER_PROCESS_HOST_H_
