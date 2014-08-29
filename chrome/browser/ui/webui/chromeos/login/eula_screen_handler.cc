// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/screens/core_oobe_actor.h"
#include "chrome/browser/chromeos/login/ui/login_web_dialog.h"
#include "chrome/browser/chromeos/login/ui/webui_login_display.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

const char kJsScreenPath[] = "login.EulaScreen";

// Helper class to tweak display details of credits pages in the context
// of OOBE/EULA step.
class CreditsWebDialog : public chromeos::LoginWebDialog {
 public:
  CreditsWebDialog(Profile* profile,
                   gfx::NativeWindow parent_window,
                   int title_id,
                   const GURL& url)
      : chromeos::LoginWebDialog(profile, NULL, parent_window,
                                 l10n_util::GetStringUTF16(title_id),
                                 url,
                                 chromeos::LoginWebDialog::STYLE_BUBBLE) {
  }

  virtual void OnLoadingStateChanged(content::WebContents* source) OVERRIDE {
    chromeos::LoginWebDialog::OnLoadingStateChanged(source);
    // Remove visual elements that we can handle in EULA page.
    bool is_loading = source->IsLoading();
    if (!is_loading && source->GetWebUI()) {
      source->GetWebUI()->CallJavascriptFunction(
          "(function () {"
          "  document.body.classList.toggle('dialog', true);"
          "  keyboard.initializeKeyboardFlow();"
          "})");
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CreditsWebDialog);
};

void ShowCreditsDialog(Profile* profile,
                       gfx::NativeWindow parent_window,
                       int title_id,
                       const GURL& credits_url) {
  CreditsWebDialog* dialog = new CreditsWebDialog(profile,
                                                  parent_window,
                                                  title_id,
                                                  credits_url);
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  dialog->SetDialogSize(l10n_util::GetLocalizedContentsWidthInPixels(
                            IDS_CREDITS_APP_DIALOG_WIDTH_PIXELS),
                        l10n_util::GetLocalizedContentsWidthInPixels(
                            IDS_CREDITS_APP_DIALOG_HEIGHT_PIXELS));
  dialog->Show();
  // The dialog object will be deleted on dialog close.
}

}  // namespace

namespace chromeos {

EulaScreenHandler::EulaScreenHandler(CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      core_oobe_actor_(core_oobe_actor),
      show_on_init_(false) {
}

EulaScreenHandler::~EulaScreenHandler() {
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void EulaScreenHandler::PrepareToShow() {
}

void EulaScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenOobeEula, NULL);
}

void EulaScreenHandler::Hide() {
}

void EulaScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EulaScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("eulaScreenTitle", IDS_EULA_SCREEN_TITLE);
  builder->Add("eulaScreenAccessibleTitle", IDS_EULA_SCREEN_ACCESSIBLE_TITLE);
  builder->Add("checkboxLogging", IDS_EULA_CHECKBOX_ENABLE_LOGGING);
  builder->Add("back", IDS_EULA_BACK_BUTTON);
  builder->Add("acceptAgreement", IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON);
  builder->Add("eulaSystemInstallationSettings",
               IDS_EULA_SYSTEM_SECURITY_SETTING);
  builder->Add("eulaTpmDesc", IDS_EULA_TPM_DESCRIPTION);
  builder->Add("eulaTpmKeyDesc", IDS_EULA_TPM_KEY_DESCRIPTION);
  builder->Add("eulaTpmDescPowerwash", IDS_EULA_TPM_KEY_DESCRIPTION_POWERWASH);
  builder->Add("eulaTpmBusy", IDS_EULA_TPM_BUSY);
  builder->Add("eulaSystemInstallationSettingsOkButton", IDS_OK);
  builder->Add("termsOfServiceLoading", IDS_TERMS_OF_SERVICE_SCREEN_LOADING);
#if defined(ENABLE_RLZ)
  builder->AddF("eulaRlzDesc",
                IDS_EULA_RLZ_DESCRIPTION,
                IDS_SHORT_PRODUCT_NAME,
                IDS_PRODUCT_NAME);
  builder->AddF("eulaRlzEnable",
                IDS_EULA_RLZ_ENABLE,
                IDS_SHORT_PRODUCT_OS_NAME);
#endif

  builder->Add("chromeCreditsLink", IDS_ABOUT_VERSION_LICENSE_EULA);
  builder->Add("chromeosCreditsLink", IDS_ABOUT_CROS_VERSION_LICENSE_EULA);
}

void EulaScreenHandler::GetAdditionalParameters(base::DictionaryValue* dict) {
#if defined(ENABLE_RLZ)
  dict->SetString("rlzEnabled", "enabled");
#else
  dict->SetString("rlzEnabled", "disabled");
#endif
}

void EulaScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  core_oobe_actor_->SetUsageStats(delegate_->IsUsageStatsEnabled());

  // This OEM EULA is a file:// URL which we're unable to load in iframe.
  // Instead if it's defined we use chrome://terms/oem that will load same file.
  if (!delegate_->GetOemEulaUrl().is_empty())
    core_oobe_actor_->SetOemEulaUrl(chrome::kChromeUITermsOemURL);

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EulaScreenHandler::RegisterMessages() {
  AddCallback("eulaOnExit", &EulaScreenHandler::HandleOnExit);
  AddCallback("eulaOnLearnMore", &EulaScreenHandler::HandleOnLearnMore);
  AddCallback("eulaOnChromeOSCredits",
              &EulaScreenHandler::HandleOnChromeOSCredits);
  AddCallback("eulaOnChromeCredits",
              &EulaScreenHandler::HandleOnChromeCredits);
  AddCallback("eulaOnLearnMore", &EulaScreenHandler::HandleOnLearnMore);
  AddCallback("eulaOnInstallationSettingsPopupOpened",
              &EulaScreenHandler::HandleOnInstallationSettingsPopupOpened);
}

void EulaScreenHandler::OnPasswordFetched(const std::string& tpm_password) {
  core_oobe_actor_->SetTpmPassword(tpm_password);
}

void EulaScreenHandler::HandleOnExit(bool accepted, bool usage_stats_enabled) {
  if (delegate_)
    delegate_->OnExit(accepted, usage_stats_enabled);
}

void EulaScreenHandler::HandleOnLearnMore() {
  if (!help_app_.get())
    help_app_ = new HelpAppLauncher(GetNativeWindow());
  help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
}

void EulaScreenHandler::HandleOnChromeOSCredits() {
  ShowCreditsDialog(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      GetNativeWindow(),
      IDS_ABOUT_CROS_VERSION_LICENSE_EULA,
      GURL(chrome::kChromeUIOSCreditsURL));
}

void EulaScreenHandler::HandleOnChromeCredits() {
  ShowCreditsDialog(
      Profile::FromBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()),
      GetNativeWindow(),
      IDS_ABOUT_VERSION_LICENSE_EULA,
      GURL(chrome::kChromeUICreditsURL));
}

void EulaScreenHandler::HandleOnInstallationSettingsPopupOpened() {
  if (delegate_)
    delegate_->InitiatePasswordFetch();
}

}  // namespace chromeos
