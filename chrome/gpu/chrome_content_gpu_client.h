// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
#define CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_

#include "base/macros.h"
#include "content/public/gpu/content_gpu_client.h"

class ChromeContentGpuClient : public content::ContentGpuClient {
 public:
  ChromeContentGpuClient();
  ~ChromeContentGpuClient() override;

  // content::ContentGpuClient implementation.
  void RegisterMojoServices(content::ServiceRegistry* registry) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeContentGpuClient);
};

#endif  // CHROME_GPU_CHROME_CONTENT_GPU_CLIENT_H_
