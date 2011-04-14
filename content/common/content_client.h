// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_CLIENT_H_
#define CONTENT_COMMON_CONTENT_CLIENT_H_
#pragma once

#include "base/basictypes.h"

class GURL;
struct GPUInfo;

namespace content {

class ContentBrowserClient;
class ContentClient;
class ContentPluginClient;
class ContentRendererClient;

// Setter and getter for the client.  The client should be set early, before any
// content code is called.
void SetContentClient(ContentClient* client);
ContentClient* GetContentClient();

// Interface that the embedder implements.
class ContentClient {
 public:
  ContentClient();
  ~ContentClient();

  ContentBrowserClient* browser() { return browser_; }
  void set_browser(ContentBrowserClient* c) { browser_ = c; }
  ContentPluginClient* plugin() { return plugin_; }
  void set_plugin(ContentPluginClient* r) { plugin_ = r; }
  ContentRendererClient* renderer() { return renderer_; }
  void set_renderer(ContentRendererClient* r) { renderer_ = r; }

  // Sets the URL that is logged if the child process crashes. Use GURL() to
  // clear the URL.
  virtual void SetActiveURL(const GURL& url) {}

  // Sets the data on the current gpu.
  virtual void SetGpuInfo(const GPUInfo& gpu_info) {}

 private:
  // The embedder API for participating in browser logic.
  ContentBrowserClient* browser_;
  // The embedder API for participating in plugin logic.
  ContentPluginClient* plugin_;
  // The embedder API for participating in renderer logic.
  ContentRendererClient* renderer_;
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_CLIENT_H_
