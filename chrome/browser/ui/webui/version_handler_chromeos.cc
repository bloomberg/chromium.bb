// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_chromeos.h"

#include "content/public/browser/web_ui.h"

VersionHandlerChromeOS::VersionHandlerChromeOS() {
}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {
}

void VersionHandlerChromeOS::HandleRequestVersionInfo(const ListValue* args) {
  // Start the asynchronous load of the version.
  loader_.GetVersion(&consumer_,
                     base::Bind(&VersionHandlerChromeOS::OnVersion,
                                base::Unretained(this)),
                     chromeos::VersionLoader::VERSION_FULL);

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerChromeOS::OnVersion(chromeos::VersionLoader::Handle handle,
                                       const std::string& version) {
  StringValue arg(version);
  web_ui()->CallJavascriptFunction("returnOsVersion", arg);
}
