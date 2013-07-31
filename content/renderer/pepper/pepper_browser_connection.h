// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "content/public/renderer/render_view_observer.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

namespace content {

class PepperHelperImpl;

// This class represents a connection from the renderer to the browser for
// sending/receiving pepper ResourceHost related messages. When the browser
// and renderer communicate about ResourceHosts, they should pass the plugin
// process ID to identify which plugin they are talking about.
class PepperBrowserConnection {
 public:
  typedef base::Callback<void(int)> PendingResourceIDCallback;
  typedef base::Callback<void(PP_FileSystemType,
                              std::string,
                              base::FilePath)> FileRefGetInfoCallback;

  explicit PepperBrowserConnection(PepperHelperImpl* helper);
  virtual ~PepperBrowserConnection();

  bool OnMessageReceived(const IPC::Message& message);

  // TODO(teravest): Instead of having separate methods per message, we should
  // add generic functionality similar to PluginResource::Call().

  // Sends a request to the browser to create a ResourceHost for the given
  // |instance| of a plugin identified by |child_process_id|. |callback| will be
  // run when a reply is received with the pending resource ID.
  void SendBrowserCreate(PP_Instance instance,
                         int child_process_id,
                         const IPC::Message& create_message,
                         const PendingResourceIDCallback& callback);

  // Sends a request to the browser to get information about the given FileRef
  // |resource|. |callback| will be run when a reply is received with the
  // file information.
  void SendBrowserFileRefGetInfo(int child_process_id,
                                 PP_Resource resource,
                                 const FileRefGetInfoCallback& callback);

  // Called when the renderer creates an in-process instance.
  void DidCreateInProcessInstance(PP_Instance instance,
                                  int render_view_id,
                                  const GURL& document_url,
                                  const GURL& plugin_url);

  // Called when the renderer deletes an in-process instance.
  void DidDeleteInProcessInstance(PP_Instance instance);

 private:
  // Message handlers.
  void OnMsgCreateResourceHostFromHostReply(int32_t sequence_number,
                                            int pending_resource_host_id);
  void OnMsgFileRefGetInfoReply(int32_t sequence_number,
                                PP_FileSystemType type,
                                std::string file_system_url_spec,
                                base::FilePath external_path);

  // Return the next sequence number.
  int32_t GetNextSequence();

  // The plugin helper that owns us.
  PepperHelperImpl* helper_;

  // Sequence number to track pending callbacks.
  int32_t next_sequence_number_;

  // Maps a sequence number to the callback to be run.
  std::map<int32_t, PendingResourceIDCallback> pending_create_map_;
  std::map<int32_t, FileRefGetInfoCallback> get_info_map_;
  DISALLOW_COPY_AND_ASSIGN(PepperBrowserConnection);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
