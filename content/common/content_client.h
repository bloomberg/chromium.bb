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

class ContentClient;
// Setter and getter for the client.  The client should be set early, before any
// content code is called.
void SetContentClient(ContentClient* client);
ContentClient* GetContentClient();

// Interface that the embedder implements.
class ContentClient {
 public:
  // Sets the URL that is logged if the child process crashes. Use GURL() to
  // clear the URL.
  virtual void SetActiveURL(const GURL& url) {}

  // Sets the data on the gpu to send along with crash reports.
  virtual void SetGpuInfo(const GPUInfo& gpu_info) {}
};

}  // namespace content

#endif  // CONTENT_COMMON_CONTENT_CLIENT_H_
