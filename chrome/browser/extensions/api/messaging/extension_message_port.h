// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_

#include "chrome/browser/extensions/api/messaging/message_service.h"

class GURL;

namespace content {
class RenderProcessHost;
}  // namespace content

namespace extensions {

// A port that manages communication with an extension.
class ExtensionMessagePort : public MessageService::MessagePort {
 public:
  ExtensionMessagePort(content::RenderProcessHost* process,
                       int routing_id,
                       const std::string& extension_id);
  void DispatchOnConnect(int dest_port_id,
                         const std::string& channel_name,
                         scoped_ptr<base::DictionaryValue> source_tab,
                         int source_frame_id,
                         int target_tab_id,
                         int target_frame_id,
                         int guest_process_id,
                         int guest_render_frame_routing_id,
                         const std::string& source_extension_id,
                         const std::string& target_extension_id,
                         const GURL& source_url,
                         const std::string& tls_channel_id) override;
  void DispatchOnDisconnect(int source_port_id,
                            const std::string& error_message) override;
  void DispatchOnMessage(const Message& message, int target_port_id) override;
  void IncrementLazyKeepaliveCount() override;
  void DecrementLazyKeepaliveCount() override;
  content::RenderProcessHost* GetRenderProcessHost() override;

 private:
  content::RenderProcessHost* process_;
  int routing_id_;
  std::string extension_id_;
  void* background_host_ptr_;  // used in IncrementLazyKeepaliveCount
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
