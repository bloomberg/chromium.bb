// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/default_settings_fetcher.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

base::string16 FormatUrlForDisplay(const GURL& url) {
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

bool ResetSearchEnabled(const SettingsResetPromptModel& model) {
  return model.default_search_reset_state() ==
         SettingsResetPromptModel::RESET_REQUIRED;
}

bool ResetStartupUrlsEnabled(const SettingsResetPromptModel& model) {
  return model.startup_urls_reset_state() ==
         SettingsResetPromptModel::RESET_REQUIRED;
}

bool ResetHomepageEnabled(const SettingsResetPromptModel& model) {
  return model.homepage_reset_state() ==
         SettingsResetPromptModel::RESET_REQUIRED;
}

void TryToShowSettingsResetPrompt(
    std::unique_ptr<SettingsResetPromptModel> model,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings) {
  DCHECK(model);
  DCHECK(default_settings);

  // Ensure that there is at least one non-incognito browser open for the
  // profile before attempting to show the dialog.
  Browser* browser = chrome::FindTabbedBrowser(
      model->profile(), /*match_original_profiles=*/false);
  if (!browser)
    return;

  // First, show the browser window, and only then show the dialog so that the
  // dialog will have focus.
  if (browser->window()->IsMinimized())
    browser->window()->Restore();
  browser->window()->Show();

  // The |SettingsResetPromptController| object will delete itself after the
  // reset prompt dialog has been closed.
  SettingsResetPromptController::ShowSettingsResetPrompt(
      browser, new SettingsResetPromptController(std::move(model),
                                                 std::move(default_settings)));
}

// Will display the settings reset prompt if required and if there is at least
// one non-incognito browser available for the corresponding profile.
void MaybeShowSettingsResetPrompt(
    std::unique_ptr<SettingsResetPromptConfig> config) {
  DCHECK(config);

  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;

  // Get the original profile in case the last active browser was incognito. We
  // ensure that there is at least one non-incognito browser open before
  // displaying the dialog.
  Profile* profile = browser->profile()->GetOriginalProfile();

  auto model = base::MakeUnique<SettingsResetPromptModel>(
      profile, std::move(config), base::MakeUnique<ProfileResetter>(profile));

  model->ReportUmaMetrics();

  if (!model->ShouldPromptForReset())
    return;

  DefaultSettingsFetcher::FetchDefaultSettings(
      base::Bind(&TryToShowSettingsResetPrompt, base::Passed(&model)));
}

class SettingsResetPromptDelegateImpl : public SettingsResetPromptDelegate {
 public:
  SettingsResetPromptDelegateImpl();
  ~SettingsResetPromptDelegateImpl() override;

  void ShowSettingsResetPromptWithDelay() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptDelegateImpl);
};

SettingsResetPromptDelegateImpl::SettingsResetPromptDelegateImpl() = default;

SettingsResetPromptDelegateImpl::~SettingsResetPromptDelegateImpl() = default;

void SettingsResetPromptDelegateImpl::ShowSettingsResetPromptWithDelay() const {
  std::unique_ptr<SettingsResetPromptConfig> config =
      SettingsResetPromptConfig::Create();
  if (!config)
    return;

  base::TimeDelta delay = config->delay_before_prompt();
  content::BrowserThread::PostDelayedTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(MaybeShowSettingsResetPrompt, base::Passed(&config)),
      delay);
}

SettingsResetPromptDelegate* g_settings_reset_prompt_delegate = nullptr;

}  // namespace.

SettingsResetPromptController::SettingsResetPromptController(
    std::unique_ptr<SettingsResetPromptModel> model,
    std::unique_ptr<BrandcodedDefaultSettings> default_settings)
    : model_(std::move(model)), default_settings_(std::move(default_settings)) {
  DCHECK(model_);
  DCHECK(model_->ShouldPromptForReset());
  DCHECK(default_settings_);

  // In the current implementation of the reset dialog, we ask users to reset
  // one settings type. If more than one setting requires reset, the model will
  // choose which setting we should prompt the user for.
  DCHECK_EQ(1, int{ResetSearchEnabled(*model_)} +
                   int{ResetStartupUrlsEnabled(*model_)} +
                   int{ResetHomepageEnabled(*model_)});

  InitMainText();
}

SettingsResetPromptController::~SettingsResetPromptController() {}

base::string16 SettingsResetPromptController::GetWindowTitle() const {
  if (ResetSearchEnabled(*model_)) {
    return l10n_util::GetStringUTF16(
        IDS_SETTINGS_RESET_PROMPT_TITLE_SEARCH_ENGINE);
  }
  if (ResetStartupUrlsEnabled(*model_)) {
    return l10n_util::GetStringUTF16(
        IDS_SETTINGS_RESET_PROMPT_TITLE_STARTUP_PAGE);
  }
  if (ResetHomepageEnabled(*model_))
    return l10n_util::GetStringUTF16(IDS_SETTINGS_RESET_PROMPT_TITLE_HOMEPAGE);

  NOTREACHED();
  return base::string16();
}

base::string16 SettingsResetPromptController::GetButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_SETTINGS_RESET_PROMPT_ACCEPT_BUTTON_LABEL);
}

base::string16 SettingsResetPromptController::GetMainText() const {
  DCHECK(!main_text_.empty());
  return main_text_;
}

gfx::Range SettingsResetPromptController::GetMainTextUrlRange() const {
  return main_text_url_range_;
}

void SettingsResetPromptController::DialogShown() {
  model_->DialogShown();
  time_dialog_shown_ = base::Time::Now();
  base::RecordAction(base::UserMetricsAction("SettingsResetPrompt_Shown"));
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.DialogShown", true);
}

void SettingsResetPromptController::Accept() {
  DCHECK(!time_dialog_shown_.is_null());
  DCHECK(default_settings_);

  base::RecordAction(base::UserMetricsAction("SettingsResetPrompt_Accepted"));
  UMA_HISTOGRAM_LONG_TIMES_100("SettingsResetPrompt.TimeUntilAccepted",
                               base::Time::Now() - time_dialog_shown_);
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.PromptAccepted", true);
  model_->PerformReset(
      std::move(default_settings_),
      base::Bind(&SettingsResetPromptController::OnInteractionDone,
                 base::Unretained(this)));
}

void SettingsResetPromptController::Cancel() {
  DCHECK(!time_dialog_shown_.is_null());
  base::RecordAction(base::UserMetricsAction("SettingsResetPrompt_Canceled"));
  UMA_HISTOGRAM_LONG_TIMES_100("SettingsResetPrompt.TimeUntilCanceled",
                               base::Time::Now() - time_dialog_shown_);
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.PromptAccepted", false);
  OnInteractionDone();
}

void SettingsResetPromptController::Close() {
  DCHECK(!time_dialog_shown_.is_null());
  base::RecordAction(base::UserMetricsAction("SettingsResetPrompt_Dismissed"));
  UMA_HISTOGRAM_LONG_TIMES_100("SettingsResetPrompt.TimeUntilDismissed",
                               base::Time::Now() - time_dialog_shown_);
  UMA_HISTOGRAM_BOOLEAN("SettingsResetPrompt.PromptAccepted", false);
  OnInteractionDone();
}

void SettingsResetPromptController::InitMainText() {
  DCHECK(main_text_.empty());

  // Get the URL string to be displayed in the dialog message.
  base::string16 url_string;
  if (ResetSearchEnabled(*model_)) {
    url_string = FormatUrlForDisplay(model_->default_search());
  } else if (ResetStartupUrlsEnabled(*model_)) {
    DCHECK(!model_->startup_urls_to_reset().empty());
    url_string = FormatUrlForDisplay(model_->startup_urls_to_reset().front());
  } else if (ResetHomepageEnabled(*model_)) {
    url_string = FormatUrlForDisplay(model_->homepage());
  } else {
    NOTREACHED();
  }

  // Get the main dialog message based on the setting that needs to be reset and
  // whether any extensions need to be disabled.
  size_t offset = 0U;
  if (ResetSearchEnabled(*model_)) {
    main_text_ = l10n_util::GetStringFUTF16(
        model_->extensions_to_disable().empty()
            ? IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_SEARCH_ENGINE_NO_EXTENSIONS
            : IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_SEARCH_ENGINE_WITH_EXTENSIONS,
        url_string, &offset);
  } else if (ResetStartupUrlsEnabled(*model_)) {
    DCHECK(!model_->startup_urls().empty());
    if (model_->startup_urls().size() == 1) {
      main_text_ = l10n_util::GetStringFUTF16(
          model_->extensions_to_disable().empty()
              ? IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_STARTUP_PAGE_SINGLE_NO_EXTENSIONS
              : IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_STARTUP_PAGE_SINGLE_WITH_EXTENSIONS,
          url_string, &offset);
    } else {  // model_->startup_urls().size() > 1
      main_text_ = l10n_util::GetStringFUTF16(
          model_->extensions_to_disable().empty()
              ? IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_STARTUP_PAGE_MULTIPLE_NO_EXTENSIONS
              : IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_STARTUP_PAGE_MULTIPLE_WITH_EXTENSIONS,
          url_string, &offset);
    }
  } else if (ResetHomepageEnabled(*model_)) {
    main_text_ = l10n_util::GetStringFUTF16(
        model_->extensions_to_disable().empty()
            ? IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_HOMEPAGE_NO_EXTENSIONS
            : IDS_SETTINGS_RESET_PROMPT_EXPLANATION_FOR_HOMEPAGE_WITH_EXTENSIONS,
        url_string, &offset);
  } else {
    NOTREACHED();
  }

  main_text_url_range_.set_start(offset);
  main_text_url_range_.set_end(offset + url_string.length());
}

void SettingsResetPromptController::OnInteractionDone() {
  delete this;
}

void MaybeShowSettingsResetPromptWithDelay() {
  if (g_settings_reset_prompt_delegate) {
    g_settings_reset_prompt_delegate->ShowSettingsResetPromptWithDelay();
  } else {
    SettingsResetPromptDelegateImpl().ShowSettingsResetPromptWithDelay();
  }
}

SettingsResetPromptDelegate::SettingsResetPromptDelegate() = default;

SettingsResetPromptDelegate::~SettingsResetPromptDelegate() = default;

void SetSettingsResetPromptDelegate(SettingsResetPromptDelegate* delegate) {
  g_settings_reset_prompt_delegate = delegate;
}

}  // namespace safe_browsing
