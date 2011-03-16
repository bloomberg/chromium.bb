// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CLIENT_H_
#define CONTENT_COMMON_CONTENT_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"

class ContentBrowserClient;
class GURL;
struct GPUInfo;

namespace content {

class ContentClient;
// Setter and getter for the client.  The client should be set early, before any
// content code is called.
void SetContentClient(ContentClient* client);
ContentClient* GetContentClient();

// Interface that the embedder implements.
class ContentClient {
 public:
  ContentClient();
  ~ContentClient();

  // Gets or sets the embedder API for participating in browser logic.
  // The client must be set early, before any content code is called.
  ContentBrowserClient* browser_client() {
    return browser_client_;
  }
  void set_browser_client(ContentBrowserClient* client) {
    browser_client_ = client;
  }

  // Sets the URL that is logged if the child process crashes. Use GURL() to
  // clear the URL.
  virtual void SetActiveURL(const GURL& url) {}

  // Sets the data on the gpu to send along with crash reports.
  virtual void SetGpuInfo(const GPUInfo& gpu_info) {}

  // Notifies that a plugin process has started.
  virtual void PluginProcessStarted(const string16& plugin_name) {}

 private:
  ContentBrowserClient* browser_client_;
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_CLIENT_H_
