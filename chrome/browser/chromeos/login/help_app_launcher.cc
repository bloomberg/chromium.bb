// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/help_app_launcher.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

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
    ShowHelpTopicDialog(profile, GURL(url));
}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, protected:

HelpAppLauncher::~HelpAppLauncher() {}

///////////////////////////////////////////////////////////////////////////////
// HelpApp, private:

void HelpAppLauncher::ShowHelpTopicDialog(Profile* profile,
                                          const GURL& topic_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LoginWebDialog* dialog = new LoginWebDialog(
      profile,
      NULL,
      parent_window_,
      l10n_util::GetStringUTF16(IDS_LOGIN_OOBE_HELP_DIALOG_TITLE),
      topic_url,
      LoginWebDialog::STYLE_BUBBLE);
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  dialog->SetDialogSize(l10n_util::GetLocalizedContentsWidthInPixels(
                            IDS_HELP_APP_DIALOG_WIDTH_PIXELS),
                        l10n_util::GetLocalizedContentsWidthInPixels(
                            IDS_HELP_APP_DIALOG_HEIGHT_PIXELS));
  dialog->Show();
  // The dialog object will be deleted on dialog close.
}

}  // namespace chromeos
