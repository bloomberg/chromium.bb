// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/sync_promo_ui.h"

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/sync_promo_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kStringsJsFile[] = "strings.js";
const char kSyncPromoJsFile[]  = "sync_promo.js";

// The Web UI data source for the sync promo page.
class SyncPromoUIHTMLSource : public ChromeWebUIDataSource {
 public:
  SyncPromoUIHTMLSource();

 private:
  ~SyncPromoUIHTMLSource() {}
  DISALLOW_COPY_AND_ASSIGN(SyncPromoUIHTMLSource);
};

SyncPromoUIHTMLSource::SyncPromoUIHTMLSource()
    : ChromeWebUIDataSource(chrome::kChromeUISyncPromoHost) {
  DictionaryValue localized_strings;
  SyncSetupHandler::GetStaticLocalizedValues(&localized_strings);
  AddLocalizedStrings(localized_strings);
}

}  // namespace

SyncPromoUI::SyncPromoUI(TabContents* contents) : ChromeWebUI(contents) {
  should_hide_url_ = true;

  SyncPromoHandler* handler = new SyncPromoHandler();
  AddMessageHandler(handler);
  handler->Attach(this);

  // Set up the chrome://theme/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  ThemeSource* theme = new ThemeSource(profile);
  profile->GetChromeURLDataManager()->AddDataSource(theme);

  // Set up the sync promo source.
  SyncPromoUIHTMLSource* html_source =
      new SyncPromoUIHTMLSource();
  html_source->set_json_path(kStringsJsFile);
  html_source->add_resource_path(kSyncPromoJsFile, IDR_SYNC_PROMO_JS);
  html_source->set_default_resource(IDR_SYNC_PROMO_HTML);
  profile->GetChromeURLDataManager()->AddDataSource(html_source);
}

bool SyncPromoUI::ShouldShowSyncPromo() {
#if defined(OS_CHROMEOS)
  // There's no need to show the sync promo on cros since cros users are logged
  // into sync already.
  return false;
#endif

  // Temporarily hide this feature behind a command line flag.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kSyncShowPromo);
}
