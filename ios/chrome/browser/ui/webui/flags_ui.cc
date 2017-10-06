// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/webui/flags_ui.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/flags_ui/flags_ui_constants.h"
#include "components/flags_ui/flags_ui_pref_names.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/grit/components_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/about_flags.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/web/public/web_ui_ios_data_source.h"
#include "ios/web/public/webui/web_ui_ios.h"
#include "ios/web/public/webui/web_ui_ios_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

web::WebUIIOSDataSource* CreateFlagsUIHTMLSource() {
  web::WebUIIOSDataSource* source =
      web::WebUIIOSDataSource::Create(kChromeUIFlagsHost);

  source->AddLocalizedString(flags_ui::kFlagsSearchPlaceholder,
                             IDS_FLAGS_UI_SEARCH_PLACEHOLDER);
  source->AddLocalizedString(flags_ui::kFlagsTitle, IDS_FLAGS_UI_TITLE);
  source->AddLocalizedString(flags_ui::kFlagsWarningHeader,
                             IDS_FLAGS_UI_WARNING_HEADER);
  source->AddLocalizedString(flags_ui::kFlagsBlurb, IDS_FLAGS_UI_WARNING_TEXT);
  source->AddLocalizedString(flags_ui::kChannelPromoBeta,
                             IDS_FLAGS_UI_PROMOTE_BETA_CHANNEL);
  source->AddLocalizedString(flags_ui::kChannelPromoDev,
                             IDS_FLAGS_UI_PROMOTE_DEV_CHANNEL);
  source->AddLocalizedString(flags_ui::kFlagsSupportedTitle,
                             IDS_FLAGS_UI_SUPPORTED_TITLE);
  source->AddLocalizedString(flags_ui::kFlagsUnsupportedTitle,
                             IDS_FLAGS_UI_UNSUPPORTED_TITLE);
  source->AddLocalizedString(flags_ui::kFlagsNotSupported,
                             IDS_FLAGS_UI_NOT_AVAILABLE);
  source->AddLocalizedString(flags_ui::kFlagsRestartNotice,
                             IDS_FLAGS_UI_RELAUNCH_NOTICE);
  source->AddLocalizedString(flags_ui::kFlagsRestartButton,
                             IDS_FLAGS_UI_RELAUNCH_BUTTON);
  source->AddLocalizedString(flags_ui::kResetAllButton,
                             IDS_FLAGS_UI_RESET_ALL_BUTTON);
  source->AddLocalizedString(flags_ui::kFlagsNoMatches,
                             IDS_FLAGS_UI_NO_MATCHES);
  source->AddLocalizedString(flags_ui::kDisable, IDS_FLAGS_UI_DISABLE);
  source->AddLocalizedString(flags_ui::kEnable, IDS_FLAGS_UI_ENABLE);
  source->AddString(flags_ui::kVersion, version_info::GetVersionNumber());

  source->SetJsonPath("strings.js");
  source->AddResourcePath(flags_ui::kFlagsJS, IDR_FLAGS_UI_FLAGS_JS);
  source->SetDefaultResource(IDR_FLAGS_UI_FLAGS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// FlagsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the about:flags page.
class FlagsDOMHandler : public web::WebUIIOSMessageHandler {
 public:
  FlagsDOMHandler()
      : access_(flags_ui::kGeneralAccessFlagsOnly),
        experimental_features_requested_(false) {}
  ~FlagsDOMHandler() override {}

  // Initializes the DOM handler with the provided flags storage and flags
  // access. If there were flags experiments requested from javascript before
  // this was called, it calls |HandleRequestExperimentalFeatures| again.
  void Init(std::unique_ptr<flags_ui::FlagsStorage> flags_storage,
            flags_ui::FlagAccess access);

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestExperimentFeatures" message.
  void HandleRequestExperimentalFeatures(const base::ListValue* args);

  // Callback for the "enableExperimentalFeature" message.
  void HandleEnableExperimentalFeatureMessage(const base::ListValue* args);

  // Callback for the "restartBrowser" message. Restores all tabs on restart.
  void HandleRestartBrowser(const base::ListValue* args);

  // Callback for the "resetAllFlags" message.
  void HandleResetAllFlags(const base::ListValue* args);

 private:
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage_;
  flags_ui::FlagAccess access_;
  bool experimental_features_requested_;

  DISALLOW_COPY_AND_ASSIGN(FlagsDOMHandler);
};

void FlagsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      flags_ui::kRequestExperimentalFeatures,
      base::Bind(&FlagsDOMHandler::HandleRequestExperimentalFeatures,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kEnableExperimentalFeature,
      base::Bind(&FlagsDOMHandler::HandleEnableExperimentalFeatureMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kRestartBrowser,
      base::Bind(&FlagsDOMHandler::HandleRestartBrowser,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      flags_ui::kResetAllFlags,
      base::Bind(&FlagsDOMHandler::HandleResetAllFlags,
                 base::Unretained(this)));
}

void FlagsDOMHandler::Init(
    std::unique_ptr<flags_ui::FlagsStorage> flags_storage,
    flags_ui::FlagAccess access) {
  flags_storage_ = std::move(flags_storage);
  access_ = access;

  if (experimental_features_requested_)
    HandleRequestExperimentalFeatures(NULL);
}

void FlagsDOMHandler::HandleRequestExperimentalFeatures(
    const base::ListValue* args) {
  experimental_features_requested_ = true;
  // Bail out if the handler hasn't been initialized yet. The request will be
  // handled after the initialization.
  if (!flags_storage_)
    return;

  base::DictionaryValue results;

  auto supported_features = base::MakeUnique<base::ListValue>();
  auto unsupported_features = base::MakeUnique<base::ListValue>();
  GetFlagFeatureEntries(flags_storage_.get(), access_, supported_features.get(),
                        unsupported_features.get());
  results.Set(flags_ui::kSupportedFeatures, std::move(supported_features));
  results.Set(flags_ui::kUnsupportedFeatures, std::move(unsupported_features));
  // Cannot restart the browser on iOS.
  results.SetBoolean(flags_ui::kNeedsRestart, false);
  results.SetBoolean(flags_ui::kShowOwnerWarning,
                     access_ == flags_ui::kGeneralAccessFlagsOnly);

  results.SetBoolean(flags_ui::kShowBetaChannelPromotion, false);
  results.SetBoolean(flags_ui::kShowDevChannelPromotion, false);
  web_ui()->CallJavascriptFunction(flags_ui::kReturnExperimentalFeatures,
                                   results);
}

void FlagsDOMHandler::HandleEnableExperimentalFeatureMessage(
    const base::ListValue* args) {
  DCHECK(flags_storage_);
  DCHECK_EQ(2u, args->GetSize());
  if (args->GetSize() != 2)
    return;

  std::string entry_internal_name;
  std::string enable_str;
  if (!args->GetString(0, &entry_internal_name) ||
      !args->GetString(1, &enable_str))
    return;

  SetFeatureEntryEnabled(flags_storage_.get(), entry_internal_name,
                         enable_str == "true");
  flags_storage_->CommitPendingWrites();
}

void FlagsDOMHandler::HandleRestartBrowser(const base::ListValue* args) {
  NOTREACHED();
}

void FlagsDOMHandler::HandleResetAllFlags(const base::ListValue* args) {
  DCHECK(flags_storage_);
  ResetAllFlags(flags_storage_.get());
  flags_storage_->CommitPendingWrites();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// FlagsUI
//
///////////////////////////////////////////////////////////////////////////////

FlagsUI::FlagsUI(web::WebUIIOS* web_ui)
    : web::WebUIIOSController(web_ui), weak_factory_(this) {
  FlagsDOMHandler* handler = new FlagsDOMHandler();
  web_ui->AddMessageHandler(base::WrapUnique(handler));

  flags_ui::FlagAccess flag_access = flags_ui::kOwnerAccessToFlags;
  handler->Init(base::MakeUnique<flags_ui::PrefServiceFlagsStorage>(
                    GetApplicationContext()->GetLocalState()),
                flag_access);

  // Set up the about:flags source.
  web::WebUIIOSDataSource::Add(ios::ChromeBrowserState::FromWebUIIOS(web_ui),
                               CreateFlagsUIHTMLSource());
}

FlagsUI::~FlagsUI() {}
