// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_SOCKET_FACTORY_H_
#define FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_SOCKET_FACTORY_H_

#include "base/callback.h"
#include "content/public/browser/devtools_socket_factory.h"

class WebEngineDevToolsSocketFactory : public content::DevToolsSocketFactory {
 public:
  using OnPortOpenedCallback = base::RepeatingCallback<void(uint16_t)>;

  explicit WebEngineDevToolsSocketFactory(OnPortOpenedCallback callback);
  ~WebEngineDevToolsSocketFactory() override;

  // content::DevToolsSocketFactory implementation.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override;
  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override;

 private:
  OnPortOpenedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WebEngineDevToolsSocketFactory);
};

#endif  // FUCHSIA_ENGINE_BROWSER_WEB_ENGINE_DEVTOOLS_SOCKET_FACTORY_H_
