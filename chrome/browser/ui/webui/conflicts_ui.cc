// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts_ui.h"

#if defined(OS_WIN)

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::UserMetricsAction;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

ChromeWebUIDataSource* CreateConflictsUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIConflictsHost);

  source->AddLocalizedString("loadingMessage", IDS_CONFLICTS_LOADING_MESSAGE);
  source->AddLocalizedString("modulesLongTitle",
                             IDS_CONFLICTS_CHECK_PAGE_TITLE_LONG);
  source->AddLocalizedString("modulesBlurb", IDS_CONFLICTS_EXPLANATION_TEXT);
  source->AddLocalizedString("moduleSuspectedBad",
                             IDS_CONFLICTS_CHECK_WARNING_SUSPECTED);
  source->AddLocalizedString("moduleConfirmedBad",
                     IDS_CONFLICTS_CHECK_WARNING_CONFIRMED);
  source->AddLocalizedString("helpCenterLink", IDS_LEARN_MORE);
  source->AddLocalizedString("investigatingText",
                             IDS_CONFLICTS_CHECK_INVESTIGATING);
  source->AddLocalizedString("modulesNoneLoaded",
                             IDS_CONFLICTS_NO_MODULES_LOADED);
  source->AddLocalizedString("headerSoftware", IDS_CONFLICTS_HEADER_SOFTWARE);
  source->AddLocalizedString("headerSignedBy", IDS_CONFLICTS_HEADER_SIGNED_BY);
  source->AddLocalizedString("headerLocation", IDS_CONFLICTS_HEADER_LOCATION);
  source->AddLocalizedString("headerVersion", IDS_CONFLICTS_HEADER_VERSION);
  source->AddLocalizedString("headerHelpTip", IDS_CONFLICTS_HEADER_HELP_TIP);
  source->set_json_path("strings.js");
  source->add_resource_path("conflicts.js", IDR_ABOUT_CONFLICTS_JS);
  source->set_default_resource(IDR_ABOUT_CONFLICTS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// ConflictsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:flags page.
class ConflictsDOMHandler : public WebUIMessageHandler,
                            public content::NotificationObserver {
 public:
  ConflictsDOMHandler() {}
  virtual ~ConflictsDOMHandler() {}

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages();

  // Callback for the "requestModuleList" message.
  void HandleRequestModuleList(const ListValue* args);

 private:
  void SendModuleList();

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details);

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ConflictsDOMHandler);
};

void ConflictsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestModuleList",
      base::Bind(&ConflictsDOMHandler::HandleRequestModuleList,
                 base::Unretained(this)));
}

void ConflictsDOMHandler::HandleRequestModuleList(const ListValue* args) {
  // This request is handled asynchronously. See Observe for when we reply back.
  registrar_.Add(this, chrome::NOTIFICATION_MODULE_LIST_ENUMERATED,
                 content::NotificationService::AllSources());
  EnumerateModulesModel::GetInstance()->ScanNow();
}

void ConflictsDOMHandler::SendModuleList() {
  EnumerateModulesModel* loaded_modules = EnumerateModulesModel::GetInstance();
  ListValue* list = loaded_modules->GetModuleList();
  DictionaryValue results;
  results.Set("moduleList", list);

  // Add the section title and the total count for bad modules found.
  int confirmed_bad = loaded_modules->confirmed_bad_modules_detected();
  int suspected_bad = loaded_modules->suspected_bad_modules_detected();
  string16 table_title;
  if (!confirmed_bad && !suspected_bad) {
    table_title += l10n_util::GetStringFUTF16(
        IDS_CONFLICTS_CHECK_PAGE_TABLE_TITLE_SUFFIX_ONE,
            base::IntToString16(list->GetSize()));
  } else {
    table_title += l10n_util::GetStringFUTF16(
        IDS_CONFLICTS_CHECK_PAGE_TABLE_TITLE_SUFFIX_TWO,
            base::IntToString16(list->GetSize()),
            base::IntToString16(confirmed_bad),
            base::IntToString16(suspected_bad));
  }
  results.SetString("modulesTableTitle", table_title);

  web_ui()->CallJavascriptFunction("returnModuleList", results);
}

void ConflictsDOMHandler::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_MODULE_LIST_ENUMERATED:
      SendModuleList();
      registrar_.RemoveAll();
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ConflictsUI
//
///////////////////////////////////////////////////////////////////////////////

ConflictsUI::ConflictsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::RecordAction(UserMetricsAction("ViewAboutConflicts"));
  web_ui->AddMessageHandler(new ConflictsDOMHandler());

  // Set up the about:conflicts source.
  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSource(profile, CreateConflictsUIHTMLSource());
}

// static
base::RefCountedMemory* ConflictsUI::GetFaviconResourceBytes() {
  return static_cast<base::RefCountedMemory*>(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          IDR_CONFLICT_FAVICON));
}

#endif
