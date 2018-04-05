// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_

#include <set>

#include "chrome/browser/devtools/protocol/forward.h"
#include "chrome/browser/devtools/protocol/target.h"
#include "net/base/host_port_pair.h"

namespace content {
class WebContents;
}

using RemoteLocations = std::set<net::HostPortPair>;

class TargetHandler : public protocol::Target::Backend {
 public:
  TargetHandler(content::WebContents* web_contents,
                protocol::UberDispatcher* dispatcher);
  ~TargetHandler() override;

  RemoteLocations& remote_locations() { return remote_locations_; }

  // Target::Backend:
  protocol::Response SetRemoteLocations(
      std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
          in_locations) override;

 private:
  RemoteLocations remote_locations_;

  DISALLOW_COPY_AND_ASSIGN(TargetHandler);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_TARGET_HANDLER_H_
