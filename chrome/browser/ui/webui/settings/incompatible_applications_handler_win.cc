// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/incompatible_applications_handler_win.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/conflicts/problematic_programs_updater_win.h"
#include "chrome/browser/conflicts/uninstall_application_win.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

IncompatibleApplicationsHandler::IncompatibleApplicationsHandler() = default;

IncompatibleApplicationsHandler::~IncompatibleApplicationsHandler() = default;

void IncompatibleApplicationsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestIncompatibleApplicationsList",
      base::BindRepeating(&IncompatibleApplicationsHandler::
                              HandleRequestIncompatibleApplicationsList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startProgramUninstallation",
      base::BindRepeating(
          &IncompatibleApplicationsHandler::HandleStartProgramUninstallation,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSubtitlePluralString",
      base::BindRepeating(
          &IncompatibleApplicationsHandler::HandleGetSubtitlePluralString,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSubtitleNoAdminRightsPluralString",
      base::BindRepeating(&IncompatibleApplicationsHandler::
                              HandleGetSubtitleNoAdminRightsPluralString,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getListTitlePluralString",
      base::BindRepeating(
          &IncompatibleApplicationsHandler::HandleGetListTitlePluralString,
          base::Unretained(this)));
}

void IncompatibleApplicationsHandler::HandleRequestIncompatibleApplicationsList(
    const base::ListValue* args) {
  CHECK_EQ(1u, args->GetList().size());
  CHECK(ProblematicProgramsUpdater::HasCachedPrograms());

  AllowJavascript();

  const base::Value& callback_id = args->GetList().front();
  ResolveJavascriptCallback(callback_id,
                            ProblematicProgramsUpdater::GetCachedPrograms());
}

void IncompatibleApplicationsHandler::HandleStartProgramUninstallation(
    const base::ListValue* args) {
  CHECK_EQ(1u, args->GetList().size());

  // Open the Apps & Settings page with the program name highlighted.
  uninstall_application::LaunchUninstallFlow(
      base::UTF8ToUTF16(args->GetList()[0].GetString()));
}

void IncompatibleApplicationsHandler::HandleGetSubtitlePluralString(
    const base::ListValue* args) {
  GetPluralString(IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_SUBPAGE_SUBTITLE,
                  args);
}

void IncompatibleApplicationsHandler::
    HandleGetSubtitleNoAdminRightsPluralString(const base::ListValue* args) {
  GetPluralString(
      IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_SUBPAGE_SUBTITLE_NO_ADMIN_RIGHTS,
      args);
}

void IncompatibleApplicationsHandler::HandleGetListTitlePluralString(
    const base::ListValue* args) {
  GetPluralString(IDS_SETTINGS_INCOMPATIBLE_APPLICATIONS_LIST_TITLE, args);
}

void IncompatibleApplicationsHandler::GetPluralString(
    int id,
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetList().size());

  const base::Value& callback_id = args->GetList()[0];
  int num_applications = args->GetList()[1].GetInt();
  DCHECK_GT(0, num_applications);

  ResolveJavascriptCallback(
      callback_id,
      base::Value(l10n_util::GetPluralStringFUTF16(id, num_applications)));
}

}  // namespace settings
