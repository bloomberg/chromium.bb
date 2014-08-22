// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

struct ExtensionHostMsg_Request_Params;

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class InfoMap;

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created on the UI thread. Messages may be handled on
// the IO thread or the UI thread.
class ExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionMessageFilter(int render_process_id,
                         content::BrowserContext* context);

  int render_process_id() { return render_process_id_; }

 private:
  friend class content::BrowserThread;
  friend class base::DeleteHelper<ExtensionMessageFilter>;

  virtual ~ExtensionMessageFilter();

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Message handlers on the UI thread.
  void OnExtensionAddListener(const std::string& extension_id,
                              const GURL& listener_url,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const GURL& listener_url,
                                 const std::string& event_name);
  void OnExtensionAddLazyListener(const std::string& extension_id,
                                  const std::string& event_name);
  void OnExtensionAttachGuest(int routing_id,
                              int element_instance_id,
                              int guest_instance_id,
                              const base::DictionaryValue& attach_params);
  void OnExtensionRemoveLazyListener(const std::string& extension_id,
                                     const std::string& event_name);
  void OnExtensionAddFilteredListener(const std::string& extension_id,
                                      const std::string& event_name,
                                      const base::DictionaryValue& filter,
                                      bool lazy);
  void OnExtensionRemoveFilteredListener(const std::string& extension_id,
                                         const std::string& event_name,
                                         const base::DictionaryValue& filter,
                                         bool lazy);
  void OnExtensionShouldSuspendAck(const std::string& extension_id,
                                   int sequence_id);
  void OnExtensionSuspendAck(const std::string& extension_id);
  void OnExtensionTransferBlobsAck(const std::vector<std::string>& blob_uuids);

  // Message handlers on the IO thread.
  void OnExtensionGenerateUniqueID(int* unique_id);
  void OnExtensionResumeRequests(int route_id);
  void OnExtensionRequestForIOThread(
      int routing_id,
      const ExtensionHostMsg_Request_Params& params);

  const int render_process_id_;

  // Should only be accessed on the UI thread.
  content::BrowserContext* browser_context_;

  scoped_refptr<extensions::InfoMap> extension_info_map_;

  // Weak pointers produced by this factory are bound to the IO thread.
  base::WeakPtrFactory<ExtensionMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_MESSAGE_FILTER_H_
