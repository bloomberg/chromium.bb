// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#define CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
#pragma once

#include "content/common/content_client.h"

namespace chrome {

class ChromeContentClient : public content::ContentClient {
 public:
  static const char* kPDFPluginName;

  virtual void SetActiveURL(const GURL& url);
  virtual void SetGpuInfo(const GPUInfo& gpu_info);
  virtual void AddPepperPlugins(std::vector<PepperPluginInfo>* plugins);
};

}  // namespace chrome

#endif  // CHROME_COMMON_CHROME_CONTENT_CLIENT_H_
