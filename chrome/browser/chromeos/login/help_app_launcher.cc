// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/help_app_launcher.h"

#include <string>

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

const char kHelpAppFormat[] =
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/oobe.html?id=%d";

}  // namespace

namespace chromeos {

///////////////////////////////////////////////////////////////////////////////
// HelpApp, public:

HelpAppLauncher::HelpAppLauncher(gfx::NativeWindow parent_window)
    : parent_window_(parent_window) {
}

void HelpAppLauncher::ShowHelpTopic(HelpTopic help_topic_id) {
  Profile* profile = ProfileHelper::GetSigninProfile();
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  DCHECK(service);
  if (!service)
    return;

  GURL url(base::StringPrintf(kHelpAppFormat,
                              static_cast<int>(help_topic_id)));
  // HelpApp component extension presents only in official builds so we can
  // show help only when the extensions is installed.
  if (service->extensions()->GetByID(url.host()))
    ShowHelpTopicDialog(GURL(url));
}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, protected:

HelpAppLauncher::~HelpAppLauncher() {}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, private:

void HelpAppLauncher::ShowHelpTopicDialog(const GURL& topic_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LoginWebDialog* dialog = new LoginWebDialog(
      NULL,
      parent_window_,
      l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE),
      topic_url,
      LoginWebDialog::STYLE_BUBBLE);
  dialog->Show();
  // The dialog object will be deleted on dialog close.
}

}  // namespace chromeos
