// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_handler_chromeos.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/ui_base_features.h"

VersionHandlerChromeOS::VersionHandlerChromeOS() : weak_factory_(this) {}

VersionHandlerChromeOS::~VersionHandlerChromeOS() {}

void VersionHandlerChromeOS::HandleRequestVersionInfo(
    const base::ListValue* args) {
  // Start the asynchronous load of the versions.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetVersion,
                 chromeos::version_loader::VERSION_FULL),
      base::Bind(&VersionHandlerChromeOS::OnVersion,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetFirmware),
      base::Bind(&VersionHandlerChromeOS::OnOSFirmware,
                 weak_factory_.GetWeakPtr()));
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&chromeos::version_loader::GetARCVersion),
      base::Bind(&VersionHandlerChromeOS::OnARCVersion,
                 weak_factory_.GetWeakPtr()));

  // Parent class takes care of the rest.
  VersionHandler::HandleRequestVersionInfo(args);
}

void VersionHandlerChromeOS::OnVersion(const std::string& version) {
  std::string os_version = version;
  // Put a string in about:version so it's easy to copy/paste into bug reports,
  // similar to the OS label on the login/lock screen in canary and dev.
  // String does not need to be localized since it is a feature name.
  if (chrome::GetChannel() <= version_info::Channel::DEV &&
      ::features::IsSingleProcessMash()) {
    os_version += " SingleProcessMash";
  }
  base::Value arg(os_version);
  web_ui()->CallJavascriptFunctionUnsafe("returnOsVersion", arg);
}

void VersionHandlerChromeOS::OnOSFirmware(const std::string& version) {
  base::Value arg(version);
  web_ui()->CallJavascriptFunctionUnsafe("returnOsFirmwareVersion", arg);
}

void VersionHandlerChromeOS::OnARCVersion(const std::string& version) {
  base::Value arg(version);
  web_ui()->CallJavascriptFunctionUnsafe("returnARCVersion", arg);
}
