// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/target_handler.h"

#include "chrome/browser/devtools/chrome_devtools_manager_delegate.h"

TargetHandler::TargetHandler(content::WebContents* web_contents,
                             protocol::UberDispatcher* dispatcher) {
  DCHECK(web_contents);
  protocol::Target::Dispatcher::wire(dispatcher, this);
}

TargetHandler::~TargetHandler() {
  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
}

protocol::Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<protocol::Target::RemoteLocation>>
        locations) {
  remote_locations_.clear();
  if (!locations)
    return protocol::Response::OK();

  for (size_t i = 0; i < locations->length(); ++i) {
    auto* item = locations->get(i);
    remote_locations_.insert(
        net::HostPortPair(item->GetHost(), item->GetPort()));
  }

  ChromeDevToolsManagerDelegate* delegate =
      ChromeDevToolsManagerDelegate::GetInstance();
  if (delegate)
    delegate->UpdateDeviceDiscovery();
  return protocol::Response::OK();
}
