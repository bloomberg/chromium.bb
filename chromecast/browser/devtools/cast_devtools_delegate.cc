// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_devtools_delegate.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "grit/shell_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

using content::DevToolsAgentHost;
using content::RenderFrameHost;

// CastDevToolsDelegate -----------------------------------------------------

CastDevToolsDelegate::CastDevToolsDelegate() {
}

CastDevToolsDelegate::~CastDevToolsDelegate() {
}

std::string CastDevToolsDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CAST_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
#endif  // defined(OS_ANDROID)
}

}  // namespace shell
}  // namespace chromecast
