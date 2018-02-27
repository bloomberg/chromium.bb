// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/incompatible_software_handler_win.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/conflicts/problematic_programs_updater_win.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

IncompatibleSoftwareHandler::IncompatibleSoftwareHandler() = default;

IncompatibleSoftwareHandler::~IncompatibleSoftwareHandler() = default;

void IncompatibleSoftwareHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestIncompatibleSoftwareList",
      base::BindRepeating(
          &IncompatibleSoftwareHandler::HandleRequestIncompatibleSoftwareList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startProgramUninstallation",
      base::BindRepeating(
          &IncompatibleSoftwareHandler::HandleStartProgramUninstallation,
          base::Unretained(this)));
}

void IncompatibleSoftwareHandler::HandleRequestIncompatibleSoftwareList(
    const base::ListValue* args) {
  CHECK_EQ(1u, args->GetList().size());
  CHECK(ProblematicProgramsUpdater::HasCachedPrograms());

  AllowJavascript();

  const base::Value& callback_id = args->GetList().front();
  ResolveJavascriptCallback(callback_id,
                            ProblematicProgramsUpdater::GetCachedPrograms());
}

void IncompatibleSoftwareHandler::HandleStartProgramUninstallation(
    const base::ListValue* args) {
  CHECK_EQ(1u, args->GetList().size());

  // TODO(pmonette): Open the Apps & Settings page with the program name
  // highlighted.
}

}  // namespace settings
