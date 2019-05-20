// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/insession_password_change_ui.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/insession_password_change_handler_chromeos.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/strings/grit/ui_strings.h"

namespace chromeos {

namespace {

PasswordChangeDialog* g_dialog = nullptr;

constexpr int kMaxDialogWidth = 768;
constexpr int kMaxDialogHeight = 640;

gfx::Size GetPasswordChangeDialogSize() {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  gfx::Size display_size = display.size();

  if (display.rotation() == display::Display::ROTATE_90 ||
      display.rotation() == display::Display::ROTATE_270) {
    display_size = gfx::Size(display_size.height(), display_size.width());
  }

  display_size = gfx::Size(std::min(display_size.width(), kMaxDialogWidth),
                           std::min(display_size.height(), kMaxDialogHeight));

  return display_size;
}

}  // namespace

PasswordChangeDialog::PasswordChangeDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIPasswordChangeUrl),
                              base::string16()) {}

PasswordChangeDialog::~PasswordChangeDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

void PasswordChangeDialog::GetDialogSize(gfx::Size* size) const {
  *size = GetPasswordChangeDialogSize();
}

void PasswordChangeDialog::Show() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (g_dialog) {
    g_dialog->Focus();
    return;
  }
  g_dialog = new PasswordChangeDialog();
  g_dialog->ShowSystemDialog();
}

InSessionPasswordChangeUI::InSessionPasswordChangeUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CHECK(profile->GetPrefs()->GetBoolean(
      prefs::kSamlInSessionPasswordChangeEnabled));
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIPasswordChangeHost);

  web_ui->AddMessageHandler(std::make_unique<InSessionPasswordChangeHandler>());

  source->SetJsonPath("strings.js");

  source->SetDefaultResource(IDR_PASSWORD_CHANGE_HTML);

  source->AddResourcePath("password_change.css", IDR_PASSWORD_CHANGE_CSS);
  source->AddResourcePath("authenticator.js",
                          IDR_PASSWORD_CHANGE_AUTHENTICATOR_JS);
  source->AddResourcePath("password_change.js", IDR_PASSWORD_CHANGE_JS);

  content::WebUIDataSource::Add(profile, source);
}

InSessionPasswordChangeUI::~InSessionPasswordChangeUI() = default;

}  // namespace chromeos
