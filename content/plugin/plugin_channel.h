// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PLUGIN_PLUGIN_CHANNEL_H_
#define CONTENT_PLUGIN_PLUGIN_CHANNEL_H_

#include <stdint.h>

#include <vector>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "content/child/npapi/np_channel_base.h"
#include "content/child/scoped_child_process_reference.h"
#include "content/plugin/webplugin_delegate_stub.h"

namespace base {
class WaitableEvent;
}

namespace content {

// Encapsulates an IPC channel between the plugin process and one renderer
// process.  On the renderer side there's a corresponding PluginChannelHost.
class PluginChannel : public NPChannelBase {
 public:
  // Get a new PluginChannel object for the current process to talk to the
  // given renderer process. The renderer ID is an opaque unique ID generated
  // by the browser.
  static PluginChannel* GetPluginChannel(
      int renderer_id,
      base::SingleThreadTaskRunner* ipc_task_runner);

  // Send a message to all renderers that the process is going to shutdown.
  static void NotifyRenderersOfPendingShutdown();

  // IPC::Listener:
  bool Send(IPC::Message* msg) override;
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelError() override;

  int renderer_id() { return renderer_id_; }

  int GenerateRouteID() override;

  // Returns the event that's set when a call to the renderer causes a modal
  // dialog to come up.
  base::WaitableEvent* GetModalDialogEvent(int render_view_id) override;

  bool in_send() { return in_send_ != 0; }

  bool incognito() { return incognito_; }
  void set_incognito(bool value) { incognito_ = value; }

#if defined(OS_POSIX)
  base::ScopedFD TakeRendererFileDescriptor() {
    return channel_->TakeClientFileDescriptor();
  }
#endif

 protected:
  ~PluginChannel() override;

  // NPChannelBase::
  void CleanUp() override;
  bool Init(base::SingleThreadTaskRunner* ipc_task_runner,
            bool create_pipe_now,
            base::WaitableEvent* shutdown_event) override;

 private:
  class MessageFilter;

  // Called on the plugin thread
  PluginChannel();

  bool OnControlMessageReceived(const IPC::Message& msg) override;

  static NPChannelBase* ClassFactory() { return new PluginChannel(); }

  void OnCreateInstance(const std::string& mime_type, int* instance_id);
  void OnDestroyInstance(int instance_id, IPC::Message* reply_msg);
  void OnGenerateRouteID(int* route_id);
  void OnClearSiteData(const std::string& site,
                       uint64_t flags,
                       uint64_t max_age);
  void OnDidAbortLoading(int render_view_id);

  std::vector<scoped_refptr<WebPluginDelegateStub> > plugin_stubs_;

  ScopedChildProcessReference process_ref_;

  // The id of the renderer who is on the other side of the channel.
  int renderer_id_;

  int in_send_;  // Tracks if we're in a Send call.
  bool log_messages_;  // True if we should log sent and received messages.
  bool incognito_; // True if the renderer is in incognito mode.
  scoped_refptr<MessageFilter> filter_;  // Handles the modal dialog events.

  // Dummy NPP value used in the plugin process to represent entities other
  // that other plugin instances for the purpose of object ownership tracking.
  scoped_ptr<struct _NPP> npp_;

  DISALLOW_COPY_AND_ASSIGN(PluginChannel);
};

}  // namespace content

#endif  // CONTENT_PLUGIN_PLUGIN_CHANNEL_H_
