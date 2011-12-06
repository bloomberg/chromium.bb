// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/aura/app_list_ui.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/aura/app_list_ui_delegate.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

static const int kPageIdOffset = 10;
static const int kIndexMask = (1 << kPageIdOffset) - 1;

// TODO(xiyuan): Merge this with the one on ntp_resource_cache.
string16 GetUrlWithLang(const GURL& url) {
  return ASCIIToUTF16(google_util::AppendGoogleLocaleParam(url).spec());
}

ChromeWebUIDataSource* CreateAppListUIHTMLSource(PrefService* prefs) {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIAppListHost);

  string16 apps = l10n_util::GetStringUTF16(IDS_NEW_TAB_APPS);
  string16 title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);

  DictionaryValue localized_strings;
  localized_strings.SetString("apps", apps);
  localized_strings.SetString("title", title);
  localized_strings.SetString("appuninstall",
      l10n_util::GetStringFUTF16(
          IDS_NEW_TAB_APP_UNINSTALL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  localized_strings.SetString("appoptions",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_OPTIONS));
  localized_strings.SetString("appcreateshortcut",
      l10n_util::GetStringUTF16(IDS_NEW_TAB_APP_CREATE_SHORTCUT));
  localized_strings.SetString("appDefaultPageName",
      l10n_util::GetStringUTF16(IDS_APP_DEFAULT_PAGE_NAME));
  localized_strings.SetString("applaunchtypepinned",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_PINNED));
  localized_strings.SetString("applaunchtyperegular",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_REGULAR));
  localized_strings.SetString("applaunchtypewindow",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));
  localized_strings.SetString("applaunchtypefullscreen",
      l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN));
  localized_strings.SetString("web_store_title",
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE));
  localized_strings.SetString("web_store_url",
      GetUrlWithLang(GURL(extension_urls::GetWebstoreLaunchURL())));

  localized_strings.SetString("search-box-hint",
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));

  // TODO(xiyuan): Maybe need to use another prefs for app list.
  int shown_page = prefs->GetInteger(prefs::kNTPShownPage);
  localized_strings.SetInteger("shown_page_type", shown_page & ~kIndexMask);
  localized_strings.SetInteger("shown_page_index", shown_page & kIndexMask);

  source->AddLocalizedStrings(localized_strings);
  source->set_json_path("strings.js");
  source->set_default_resource(IDR_APP_LIST_HTML);
  return source;
}

class AppListHandler : public WebUIMessageHandler {
 public:
  AppListHandler() {}
  virtual ~AppListHandler() {}

  // WebUIMessageHandler
  virtual void RegisterMessages() OVERRIDE;

 private:
  AppListUI* app_list_ui() const {
    return static_cast<AppListUI*>(web_ui_);
  }

  void HandleClose(const base::ListValue* args);
  void HandleAppsLoaded(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(AppListHandler);
};

void AppListHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("close",
      base::Bind(&AppListHandler::HandleClose, base::Unretained(this)));
  web_ui_->RegisterMessageCallback("onAppsLoaded",
      base::Bind(&AppListHandler::HandleAppsLoaded, base::Unretained(this)));
}

void AppListHandler::HandleClose(const base::ListValue* args) {
  if (app_list_ui()->delegate())
    app_list_ui()->delegate()->Close();
}

void AppListHandler::HandleAppsLoaded(const base::ListValue* args) {
  if (app_list_ui()->delegate())
    app_list_ui()->delegate()->OnAppsLoaded();
}

}  // namespace

AppListUI::AppListUI(TabContents* contents)
    : ChromeWebUI(contents),
      delegate_(NULL) {
  AddMessageHandler((new AppListHandler)->Attach(this));

  ExtensionService* service = GetProfile()->GetExtensionService();
  if (service)
    AddMessageHandler((new AppLauncherHandler(service))->Attach(this));

  // Set up the source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  PrefService* prefs = profile->GetPrefs();
  profile->GetChromeURLDataManager()->AddDataSource(
      CreateAppListUIHTMLSource(prefs));
}
