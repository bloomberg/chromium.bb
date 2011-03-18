// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#define CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include "content/renderer/content_renderer_client.h"

namespace chrome {

class ChromeContentRendererClient : public content::ContentRendererClient {
 public:
  virtual SkBitmap* GetSadPluginBitmap();
  virtual std::string GetDefaultEncoding();
};

}  // namespace chrome

#endif  // CHROME_RENDERER_CHROME_CONTENT_RENDERER_CLIENT_H_
