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
  virtual void DispatchOnConnect(
      int dest_port_id,
      const std::string& channel_name,
      const base::DictionaryValue& source_tab,
      const std::string& source_extension_id,
      const std::string& target_extension_id,
      const GURL& source_url,
      const std::string& tls_channel_id) OVERRIDE;
  virtual void DispatchOnDisconnect(int source_port_id,
                                    const std::string& error_message) OVERRIDE;
  virtual void DispatchOnMessage(const Message& message,
                                 int target_port_id) OVERRIDE;
  virtual void IncrementLazyKeepaliveCount() OVERRIDE;
  virtual void DecrementLazyKeepaliveCount() OVERRIDE;
  virtual content::RenderProcessHost* GetRenderProcessHost() OVERRIDE;

 private:
  content::RenderProcessHost* process_;
  int routing_id_;
  std::string extension_id_;
  void* background_host_ptr_;  // used in IncrementLazyKeepaliveCount
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_EXTENSION_MESSAGE_PORT_H_
