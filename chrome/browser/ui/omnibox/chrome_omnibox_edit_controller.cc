// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/toolbar/toolbar_model.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#endif

void ChromeOmniboxEditController::OnAutocompleteAccept(
    const GURL& destination_url,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type) {
  OmniboxEditController::OnAutocompleteAccept(destination_url, disposition,
                                              transition, match_type);
  if (command_updater_)
    command_updater_->ExecuteCommand(IDC_OPEN_CURRENT_URL);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extensions::MaybeShowExtensionControlledSearchNotification(
      GetWebContents(), match_type);
#endif
}

void ChromeOmniboxEditController::OnInputInProgress(bool in_progress) {
  GetToolbarModel()->set_input_in_progress(in_progress);
  UpdateWithoutTabRestore();
}

bool ChromeOmniboxEditController::SwitchToTabWithURL(const std::string& url,
                                                     bool close_this) {
#if !(defined(OS_ANDROID) || defined(OS_IOS))
  Profile* profile =
      Profile::FromBrowserContext(GetWebContents()->GetBrowserContext());
  if (!profile)
    return false;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->IsSameProfile(profile) &&
        browser->profile()->GetProfileType() == profile->GetProfileType()) {
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* new_web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        if (new_web_contents->GetLastCommittedURL() == url) {
          content::WebContents* old_web_contents = GetWebContents();
          if (close_this) {
            old_web_contents->Close();
          } else {
            // Transfer focus, from the Omnibox, to the tab so that when
            // focus comes back to the tab, it's focused on the content.
            // TODO(crbug.com/46623): No idea how appropriate this approach
            // is.  I have this feeling that we should add another command
            // to BrowserCommandController::ExecuteCommandWithDisposition().
            old_web_contents->Focus();
          }
          if (new_web_contents != old_web_contents)
            new_web_contents->GetDelegate()->ActivateContents(new_web_contents);
          return true;
        }
      }
    }
  }
#endif  // !(defined(OS_ANDROID) || defined(OS_IOS))
  return false;
}

ChromeOmniboxEditController::ChromeOmniboxEditController(
    CommandUpdater* command_updater)
    : command_updater_(command_updater) {}

ChromeOmniboxEditController::~ChromeOmniboxEditController() {}
