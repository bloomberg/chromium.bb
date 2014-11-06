// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/utility/utility_handler.h"

#include "base/command_line.h"
#include "content/public/utility/utility_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/extension_utility_messages.h"
#include "extensions/common/update_manifest.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/ui_base_switches.h"

namespace extensions {

namespace {

bool Send(IPC::Message* message) {
  return content::UtilityThread::Get()->Send(message);
}

void ReleaseProcessIfNeeded() {
  content::UtilityThread::Get()->ReleaseProcessIfNeeded();
}

}  // namespace

UtilityHandler::UtilityHandler() {
}

UtilityHandler::~UtilityHandler() {
}

// static
void UtilityHandler::UtilityThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string lang = command_line->GetSwitchValueASCII(switches::kLang);
  if (!lang.empty())
    extension_l10n_util::SetProcessLocale(lang);
}

bool UtilityHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(UtilityHandler, message)
    IPC_MESSAGE_HANDLER(ExtensionUtilityMsg_ParseUpdateManifest,
                        OnParseUpdateManifest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void UtilityHandler::OnParseUpdateManifest(const std::string& xml) {
  UpdateManifest manifest;
  if (!manifest.Parse(xml)) {
    Send(new ExtensionUtilityHostMsg_ParseUpdateManifest_Failed(
        manifest.errors()));
  } else {
    Send(new ExtensionUtilityHostMsg_ParseUpdateManifest_Succeeded(
        manifest.results()));
  }
  ReleaseProcessIfNeeded();
}

}  // namespace extensions
