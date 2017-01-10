// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts_ui.h"

#if defined(OS_WIN)

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/scoped_observer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/win/enumerate_modules_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

using base::UserMetricsAction;
using content::WebContents;
using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateConflictsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIConflictsHost);

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
  source->SetJsonPath("strings.js");
  source->AddResourcePath("conflicts.js", IDR_ABOUT_CONFLICTS_JS);
  source->SetDefaultResource(IDR_ABOUT_CONFLICTS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// ConflictsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for JavaScript messages for the about:conflicts page.
class ConflictsDOMHandler : public WebUIMessageHandler,
                            public EnumerateModulesModel::Observer {
 public:
  ConflictsDOMHandler();
  ~ConflictsDOMHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestModuleList" message.
  void HandleRequestModuleList(const base::ListValue* args);

 private:
  void SendModuleList();

  // EnumerateModulesModel::Observer implementation.
  void OnScanCompleted() override;

  ScopedObserver<EnumerateModulesModel,
                 EnumerateModulesModel::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(ConflictsDOMHandler);
};

ConflictsDOMHandler::ConflictsDOMHandler()
    : observer_(this) {
}

void ConflictsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestModuleList",
      base::Bind(&ConflictsDOMHandler::HandleRequestModuleList,
                 base::Unretained(this)));
}

void ConflictsDOMHandler::HandleRequestModuleList(const base::ListValue* args) {
  // The request is handled asynchronously, and will callback via
  // OnScanCompleted on completion.
  auto* model = EnumerateModulesModel::GetInstance();

  // The JS shouldn't be abusive and call 'requestModuleList' twice, but it's
  // easy enough to defend against this.
  if (!observer_.IsObserving(model)) {
    observer_.Add(model);

    // Ask the scan to be performed immediately, and not in background mode.
    // This ensures the results are available ASAP for the UI.
    model->ScanNow(false);
  }
}

void ConflictsDOMHandler::SendModuleList() {
  auto* loaded_modules = EnumerateModulesModel::GetInstance();
  base::ListValue* list = loaded_modules->GetModuleList();
  base::DictionaryValue results;
  results.Set("moduleList", list);

  // Add the section title and the total count for bad modules found.
  int confirmed_bad = loaded_modules->confirmed_bad_modules_detected();
  int suspected_bad = loaded_modules->suspected_bad_modules_detected();
  base::string16 table_title;
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

  web_ui()->CallJavascriptFunctionUnsafe("returnModuleList", results);
}

void ConflictsDOMHandler::OnScanCompleted() {
  SendModuleList();
  observer_.Remove(EnumerateModulesModel::GetInstance());
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ConflictsUI
//
///////////////////////////////////////////////////////////////////////////////

ConflictsUI::ConflictsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::RecordAction(UserMetricsAction("ViewAboutConflicts"));
  web_ui->AddMessageHandler(base::MakeUnique<ConflictsDOMHandler>());

  // Set up the about:conflicts source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateConflictsUIHTMLSource());
}

// static
base::RefCountedMemory* ConflictsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_CONFLICT_FAVICON, scale_factor));
}

#endif
