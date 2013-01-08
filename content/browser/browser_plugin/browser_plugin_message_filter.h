// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

struct ViewHostMsg_CreateWindow_Params;

namespace content {
class BrowserContext;

// This class filters out incoming IPC messages for the guest renderer process
// on the IPC thread before other message filters handle them.
class BrowserPluginMessageFilter : public BrowserMessageFilter {
 public:
  BrowserPluginMessageFilter(int render_process_id,
                             BrowserContext* browser_context);

  // BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~BrowserPluginMessageFilter();
  void OnCreateWindow(const IPC::Message& message);

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginMessageFilter);
};

} // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_MESSAGE_FILTER_H_
