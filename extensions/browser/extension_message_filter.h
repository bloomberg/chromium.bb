// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_RENDER_MESSAGE_FILTER_H_
#define EXTENSIONS_BROWSER_EXTENSION_RENDER_MESSAGE_FILTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/browser_message_filter.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// This class filters out incoming extension-specific IPC messages from the
// renderer process. It is created on the UI thread. Messages may be handled on
// the IO thread or the UI thread.
class ExtensionMessageFilter : public content::BrowserMessageFilter {
 public:
  ExtensionMessageFilter(int render_process_id,
                         content::BrowserContext* context);

 private:
  virtual ~ExtensionMessageFilter();

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  // Message handlers on the UI thread.
  void OnExtensionAddListener(const std::string& extension_id,
                              const std::string& event_name);
  void OnExtensionRemoveListener(const std::string& extension_id,
                                 const std::string& event_name);
  void OnExtensionAddLazyListener(const std::string& extension_id,
                                  const std::string& event_name);
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

  // Message handlers on the IO thread.
  void OnExtensionGenerateUniqueID(int* unique_id);
  void OnExtensionResumeRequests(int route_id);

  const int render_process_id_;

  // Should only be accessed on the UI thread.
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageFilter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_RENDER_MESSAGE_FILTER_H_
