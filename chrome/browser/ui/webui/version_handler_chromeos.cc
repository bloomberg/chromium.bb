// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_chromeos.h"

#include "base/bind.h"
#include "content/public/browser/web_ui.h"

VersionHandlerChromeOS::VersionHandlerChromeOS() {
}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {
}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::ListValue* args) {
  // Start the asynchronous load of the version.
  loader_.GetVersion(
      chromeos::VersionLoader::VERSION_FULL,
      base::Bind(&VersionHandlerChromeOS::OnVersion, base::Unretained(this)),
      &tracker_);

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerChromeOS::OnVersion(const std::string& version) {
  base::StringValue arg(version);
  web_ui()->CallJavascriptFunction("returnOsVersion", arg);
}
