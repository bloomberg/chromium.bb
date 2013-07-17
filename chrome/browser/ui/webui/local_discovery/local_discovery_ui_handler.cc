// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

LocalDiscoveryUIHandler::LocalDiscoveryUIHandler()
    : action_callback_(base::Bind(&LocalDiscoveryUIHandler::OnNewDevice,
                                  base::Unretained(this))) {
  content::AddActionCallback(action_callback_);
}

LocalDiscoveryUIHandler::~LocalDiscoveryUIHandler() {
  content::RemoveActionCallback(action_callback_);
}

void LocalDiscoveryUIHandler::RegisterMessages() {}

void LocalDiscoveryUIHandler::OnNewDevice(const std::string& name) {
  // TODO(gene): Once we receive information about new locally discovered
  // device, we should add it to the page.
  // Here is an example how to do it:
  //
  // base::StringValue service_name(name);
  //
  // base::DictionaryValue info;
  // info.SetString("domain", "domain.local");
  // info.SetString("port", "80");
  // info.SetString("ip", "XXX.XXX.XXX.XXX");
  // info.SetString("metadata", "metadata\nmetadata");
  // info.SetString("lastSeen", "unknown");
  // info.SetString("registered", "not registered");
  //
  // base::StringValue domain("domain.local");
  // base::StringValue port("80");
  // base::StringValue ip("XXX.XXX.XXX.XXX");
  // base::StringValue metadata("metadata<br>metadata");
  // base::StringValue lastSeen("unknown");
  // base::StringValue registered("not registered");
  //
  // web_ui()->CallJavascriptFunction("onServiceUpdate", service_name, info);
}
