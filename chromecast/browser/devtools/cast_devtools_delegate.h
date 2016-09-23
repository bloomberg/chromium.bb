// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_DELEGATE_H_
#define CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_DELEGATE_H_

#include "base/macros.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {
class DevToolsAgentHost;
}

namespace chromecast {
namespace shell {

class CastDevToolsDelegate :
    public content::DevToolsManagerDelegate {
 public:
  CastDevToolsDelegate();
  ~CastDevToolsDelegate() override;

  // content::DevToolsManagerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastDevToolsDelegate);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DEVTOOLS_CAST_DEVTOOLS_DELEGATE_H_
