// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"

#include "base/json/json_writer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

void AddInternetStrings(content::WebUIDataSource* html_source) {
  // Add default strings first.
  chromeos::network_element::AddLocalizedStrings(html_source);
  chromeos::network_element::AddOncLocalizedStrings(html_source);
  chromeos::network_element::AddConfigLocalizedStrings(html_source);
  chromeos::network_element::AddErrorLocalizedStrings(html_source);
  // Add additional strings and overrides needed by the dialog.
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"cancel", IDS_CANCEL},
      {"save", IDS_SAVE},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

}  // namespace

// static
void InternetConfigDialog::ShowDialogForNetworkState(
    const NetworkState* network_state) {
  std::string network_type =
      chromeos::network_util::TranslateShillTypeToONC(network_state->type());
  InternetConfigDialog* dialog =
      new InternetConfigDialog(network_type, network_state->guid());
  dialog->ShowSystemDialog();
}

// static
void InternetConfigDialog::ShowDialogForNetworkType(
    const std::string& network_type) {
  InternetConfigDialog* dialog = new InternetConfigDialog(network_type, "");
  dialog->ShowSystemDialog();
}

InternetConfigDialog::InternetConfigDialog(const std::string& network_type,
                                           const std::string& network_id)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIIntenetConfigDialogURL),
                              base::string16() /* title */),
      network_type_(network_type),
      network_id_(network_id) {}

InternetConfigDialog::~InternetConfigDialog() {}

std::string InternetConfigDialog::GetDialogArgs() const {
  base::DictionaryValue args;
  args.SetKey("type", base::Value(network_type_));
  args.SetKey("guid", base::Value(network_id_));
  std::string json;
  base::JSONWriter::Write(args, &json);
  return json;
}

// InternetConfigDialogUI

InternetConfigDialogUI::InternetConfigDialogUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUIInternetConfigDialogHost);

  AddInternetStrings(source);
  source->AddLocalizedString("title", IDS_SETTINGS_INTERNET_CONFIG);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_INTERNET_CONFIG_DIALOG_HTML);
  source->AddResourcePath("internet_config_dialog.js",
                          IDR_INTERNET_CONFIG_DIALOG_JS);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

InternetConfigDialogUI::~InternetConfigDialogUI() {}

}  // namespace chromeos
