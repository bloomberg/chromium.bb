// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_

#include <map>

#include "base/callback.h"
#include "content/public/renderer/render_view_observer.h"
#include "ppapi/c/pp_instance.h"

namespace content {

class PepperPluginDelegateImpl;

// This class represents a connection from the renderer to the browser for
// sending/receiving pepper ResourceHost related messages. When the browser
// and renderer communicate about ResourceHosts, they should pass the plugin
// process ID to identify which plugin they are talking about.
class PepperBrowserConnection {
 public:
  typedef base::Callback<void(int)> PendingResourceIDCallback;

  explicit PepperBrowserConnection(PepperPluginDelegateImpl* plugin_delegate);
  virtual ~PepperBrowserConnection();

  bool OnMessageReceived(const IPC::Message& message);

  // Sends a request to the browser to create a ResourceHost for the given
  // |instance| of a plugin identified by |child_process_id|. |callback| will be
  // run when a reply is received with the pending resource ID.
  void SendBrowserCreate(PP_Instance instance,
                         int child_process_id,
                         const IPC::Message& create_message,
                         const PendingResourceIDCallback& callback);

 private:
  // Message handlers.
  void OnMsgCreateResourceHostFromHostReply(int32_t sequence_number,
                                            int pending_resource_host_id);

  // Return the next sequence number.
  int32_t GetNextSequence();

  // The plugin delegate that owns us.
  PepperPluginDelegateImpl* plugin_delegate_;

  // Sequence number to track pending callbacks.
  int32_t next_sequence_number_;

  // Maps a sequence number to the callback to be run.
  std::map<int32_t, PendingResourceIDCallback> pending_create_map_;

  DISALLOW_COPY_AND_ASSIGN(PepperBrowserConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
