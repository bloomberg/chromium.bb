// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <string>

#include "content/common/content_client.h"

class SkBitmap;

namespace content {

// Embedder API for participating in renderer logic.
class ContentRendererClient {
 public:
  virtual SkBitmap* GetSadPluginBitmap();
  virtual std::string GetDefaultEncoding();
};

}  // namespace content

#endif  // CONTENT_RENDERER_CONTENT_RENDERER_CLIENT_H_
