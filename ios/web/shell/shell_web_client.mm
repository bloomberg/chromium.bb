// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/shell/shell_web_client.h"

#include "ios/web/public/user_agent.h"
#include "ios/web/shell/shell_web_main_parts.h"

namespace web {

ShellWebClient::ShellWebClient() {
}

ShellWebClient::~ShellWebClient() {
}

WebMainParts* ShellWebClient::CreateWebMainParts() {
  web_main_parts_.reset(new ShellWebMainParts);
  return web_main_parts_.get();
}

ShellBrowserState* ShellWebClient::browser_state() const {
  return web_main_parts_->browser_state();
}

std::string ShellWebClient::GetProduct() const {
  return "CriOS/36.77.34.45";
}

std::string ShellWebClient::GetUserAgent(bool desktop_user_agent) const {
  std::string product = GetProduct();
  return web::BuildUserAgentFromProduct(product);
}

}  // namespace web
