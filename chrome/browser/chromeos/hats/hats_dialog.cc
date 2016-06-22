// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/hats/hats_dialog.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const int kDefaultWidth = 400;
const int kDefaultHeight = 420;
constexpr char kReplacementToken[] = "$SCRIPT_SRC";
// Site ID for the Google consumer HaTS survey.
// TODO(malaykeshav): Replace this demo survey with actual site id.
constexpr char kSiteID[] = "ckvqucibdlzn2";
// Base URL to fetch the google consumer survey script.
constexpr char kBaseFormatUrl[] =
    "https://www.google.com/insights/consumersurveys/"
    "async_survey?site=%s&force_https=1";

std::string LoadLocalHtmlAsString() {
  std::string html_data;
  ResourceBundle::GetSharedInstance()
      .GetRawDataResource(IDR_HATS_HTML)
      .CopyToString(&html_data);
  size_t pos = html_data.find(kReplacementToken);
  html_data.replace(pos, strlen(kReplacementToken),
                    base::StringPrintf(kBaseFormatUrl, kSiteID));
  return html_data;
}

}  // namespace

HatsDialog::HatsDialog() : html_data_(LoadLocalHtmlAsString()) {}

HatsDialog::~HatsDialog() {}

void HatsDialog::Show() {
  chrome::ShowWebDialog(
      nullptr, ProfileManager::GetActiveUserProfile()->GetOffTheRecordProfile(),
      this);
}

ui::ModalType HatsDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

base::string16 HatsDialog::GetDialogTitle() const {
  return base::string16();
}

GURL HatsDialog::GetDialogContentURL() const {
  return GURL("data:text/html;charset=utf-8," + html_data_);
}

void HatsDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {}

void HatsDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

bool HatsDialog::CanResizeDialog() const {
  return false;
}

std::string HatsDialog::GetDialogArgs() const {
  return std::string();
}

void HatsDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void HatsDialog::OnCloseContents(WebContents* source, bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool HatsDialog::ShouldShowDialogTitle() const {
  return false;
}

bool HatsDialog::HandleContextMenu(const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

}  // namespace chromeos
