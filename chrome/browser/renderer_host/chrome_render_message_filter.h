// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
#pragma once

#include "content/browser/browser_message_filter.h"

class ResourceDispatcherHost;

// This class filters out incoming Chrome-specific IPC messages for the renderer
// process on the IPC thread.
class ChromeRenderMessageFilter : public BrowserMessageFilter {
 public:
  ChromeRenderMessageFilter();

  // BrowserMessageFilter methods:
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);
  virtual void OnDestruct() const;

 private:
  friend class BrowserThread;
  friend class DeleteTask<ChromeRenderMessageFilter>;

  virtual ~ChromeRenderMessageFilter();
  
  void OnLaunchNaCl(const std::wstring& url,
                    int channel_descriptor,
                    IPC::Message* reply_msg);
  void OnDnsPrefetch(const std::vector<std::string>& hostnames);
  void OnRendererHistograms(int sequence_number,
                            const std::vector<std::string>& histogram_info);

  ResourceDispatcherHost* resource_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderMessageFilter);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_MESSAGE_FILTER_H_
