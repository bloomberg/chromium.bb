// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_content_renderer_client.h"

#include "grit/renderer_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chrome {

SkBitmap* ChromeContentRendererClient::GetSadPluginBitmap() {
  return ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SAD_PLUGIN);
}

}  // namespace chrome
