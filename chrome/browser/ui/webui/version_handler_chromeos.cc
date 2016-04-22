// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_chromeos.h"

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

VersionHandlerChromeOS::VersionHandlerChromeOS()
    : weak_factory_(this) {
}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {
}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::ListValue* args) {
  // Start the asynchronous load of the versions.
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&VersionHandlerChromeOS::OnVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&chromeos::version_loader::GetARCVersion),
      base::Bind(&VersionHandlerChromeOS::OnARCVersion,
                 weak_factory_.GetWeakPtr()));

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerChromeOS::OnVersion(const std::string& version) {
  base::StringValue arg(version);
  web_ui()->CallJavascriptFunction("returnOsVersion", arg);
}

void VersionHandlerChromeOS::OnARCVersion(const std::string& version) {
  base::StringValue arg(version);
  web_ui()->CallJavascriptFunction("returnARCVersion", arg);
}
