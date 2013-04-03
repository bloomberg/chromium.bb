// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

class BrowserContext;
class BrowserPluginGuestManager;

// This class filters out incoming IPC messages for the guest renderer process
// on the IPC thread before other message filters handle them.
class BrowserPluginMessageFilter : public BrowserMessageFilter {
 public:
  BrowserPluginMessageFilter(int render_process_id, bool is_guest);

  // BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<BrowserPluginMessageFilter>;

  virtual ~BrowserPluginMessageFilter();

  BrowserPluginGuestManager* GetBrowserPluginGuestManager();

  int render_process_id_;
  int is_guest_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginMessageFilter);
};

} // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_
