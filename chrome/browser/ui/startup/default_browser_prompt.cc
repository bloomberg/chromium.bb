// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"

#include <string>

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/user_metrics.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// The delegate for the infobar shown when Chrome is not the default browser.
// Ownership of the delegate is given to the infobar itself, the lifetime of
// which is bound to the containing WebContents.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a default browser infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service, Profile* profile);

 private:
  // Possible user interactions with the default browser info bar.
  // Do not modify the ordering as it is important for UMA.
  enum InfoBarUserInteraction {
    // The user clicked the "OK" (i.e., "Set as default") button.
    ACCEPT_INFO_BAR = 0,
    // The user clicked the "Cancel" (i.e., "Don't ask again") button.
    CANCEL_INFO_BAR = 1,
    // The user did not interact with the info bar.
    IGNORE_INFO_BAR = 2,
    NUM_INFO_BAR_USER_INTERACTION_TYPES
  };

  explicit DefaultBrowserInfoBarDelegate(Profile* profile);
  ~DefaultBrowserInfoBarDelegate() override;

  void AllowExpiry() { should_expire_ = true; }

  // InfoBarDelegate:
  Type GetInfoBarType() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  gfx::VectorIconId GetVectorIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool OKButtonTriggersUACPrompt() const override;
  bool Accept() override;
  bool Cancel() override;

  // The WebContents's corresponding profile.
  Profile* profile_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_ = false;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<DefaultBrowserInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegate);
};

// static
void DefaultBrowserInfoBarDelegate::Create(InfoBarService* infobar_service,
                                           Profile* profile) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new DefaultBrowserInfoBarDelegate(profile))));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(Profile* profile)
    : ConfirmInfoBarDelegate(), profile_(profile), weak_factory_(this) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&DefaultBrowserInfoBarDelegate::AllowExpiry,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() = default;

infobars::InfoBarDelegate::Type DefaultBrowserInfoBarDelegate::GetInfoBarType()
    const {
#if defined(OS_WIN)
  return WARNING_TYPE;
#else
  return PAGE_ACTION_TYPE;
#endif
}

infobars::InfoBarDelegate::InfoBarIdentifier
DefaultBrowserInfoBarDelegate::GetIdentifier() const {
  return DEFAULT_BROWSER_INFOBAR_DELEGATE;
}

int DefaultBrowserInfoBarDelegate::GetIconId() const {
  return IDR_PRODUCT_LOGO_32;
}

gfx::VectorIconId DefaultBrowserInfoBarDelegate::GetVectorIconId() const {
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#else
  return gfx::VectorIconId::CHROME_PRODUCT;
#endif
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return should_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

void DefaultBrowserInfoBarDelegate::InfoBarDismissed() {
  content::RecordAction(
      base::UserMetricsAction("DefaultBrowserInfoBar_Dismiss"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            IGNORE_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
}

base::string16 DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
}

base::string16 DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
#if defined(OS_WIN)
  // On Windows 10, the "OK" button opens the Windows Settings application,
  // through which the user must make their default browser choice.
  const int kSetAsDefaultButtonMessageId =
      base::win::GetVersion() >= base::win::VERSION_WIN10
          ? IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL_WIN_10
          : IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL;
#else
  const int kSetAsDefaultButtonMessageId =
      IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL;
#endif
  return l10n_util::GetStringUTF16(
      button == BUTTON_OK ? kSetAsDefaultButtonMessageId
                          : IDS_DEFAULT_BROWSER_INFOBAR_CANCEL_BUTTON_LABEL);
}

// Setting an app as the default browser doesn't require elevation directly, but
// it does require registering it as the protocol handler for "http", so if
// protocol registration in general requires elevation, this does as well.
bool DefaultBrowserInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return shell_integration::IsElevationNeededForSettingDefaultProtocolClient();
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  content::RecordAction(
      base::UserMetricsAction("DefaultBrowserInfoBar_Accept"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            ACCEPT_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
  // The worker pointer is reference counted. While it is running, the
  // message loops of the FILE and UI thread will hold references to it
  // and it will be automatically freed once all its tasks have finished.
  scoped_refptr<shell_integration::DefaultBrowserWorker>(
      new shell_integration::DefaultBrowserWorker(
          shell_integration::DefaultWebClientWorkerCallback()))
      ->StartSetAsDefault();
  return true;
}

bool DefaultBrowserInfoBarDelegate::Cancel() {
  chrome::DefaultBrowserPromptDeclined(profile_);
  content::RecordAction(
      base::UserMetricsAction("DefaultBrowserInfoBar_Cancel"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            CANCEL_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
  return true;
}

void ResetCheckDefaultBrowserPref(const base::FilePath& profile_path) {
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  if (profile)
    chrome::ResetDefaultBrowserPrompt(profile);
}

void ShowPrompt() {
  Browser* browser = chrome::FindLastActive();
  if (!browser)
    return;  // Reached during ui tests.

  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  DefaultBrowserInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents), browser->profile());
}

// Returns true if the default browser prompt should be shown if Chrome is not
// the user's default browser.
bool ShouldShowDefaultBrowserPrompt(Profile* profile) {
  // Do not show the prompt if the "suppress_default_browser_prompt_for_version"
  // master preference is set to the current version.
  const std::string disable_version_string =
      g_browser_process->local_state()->GetString(
          prefs::kBrowserSuppressDefaultBrowserPrompt);
  const Version disable_version(disable_version_string);
  DCHECK(disable_version_string.empty() || disable_version.IsValid());
  if (disable_version.IsValid() &&
      disable_version == Version(version_info::GetVersionNumber())) {
    return false;
  }

  // Do not show if the prompt period has yet to pass since the user previously
  // dismissed the infobar.
  int64_t last_dismissed_value =
      profile->GetPrefs()->GetInt64(prefs::kDefaultBrowserLastDeclined);
  if (last_dismissed_value) {
    int period_days = 0;
    base::StringToInt(variations::GetVariationParamValue(
                          "DefaultBrowserInfobar", "RefreshPeriodDays"),
                      &period_days);
    if (period_days <= 0 || period_days == std::numeric_limits<int>::max())
      return false;  // Failed to parse a reasonable period.
    base::Time show_on_or_after =
        base::Time::FromInternalValue(last_dismissed_value) +
        base::TimeDelta::FromDays(period_days);
    if (base::Time::Now() < show_on_or_after)
      return false;
  }

  return true;
}

void OnCheckIsDefaultBrowserFinished(
    const base::FilePath& profile_path,
    bool show_prompt,
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    ResetCheckDefaultBrowserPref(profile_path);
  } else if (show_prompt && state == shell_integration::NOT_DEFAULT &&
             shell_integration::CanSetAsDefaultBrowser()) {
    ShowPrompt();
  }
}

}  // namespace

namespace chrome {

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kBrowserSuppressDefaultBrowserPrompt, std::string());
}

void ShowDefaultBrowserPrompt(Profile* profile) {
  // Do not check if Chrome is the default browser if there is a policy in
  // control of this setting.
  if (g_browser_process->local_state()->IsManagedPreference(
      prefs::kDefaultBrowserSettingEnabled)) {
    // Handling of the browser.default_browser_setting_enabled policy setting is
    // taken care of in BrowserProcessImpl.
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  // Reset preferences if kResetCheckDefaultBrowser is true.
  if (prefs->GetBoolean(prefs::kResetCheckDefaultBrowser)) {
    prefs->SetBoolean(prefs::kResetCheckDefaultBrowser, false);
    ResetDefaultBrowserPrompt(profile);
  }

  scoped_refptr<shell_integration::DefaultBrowserWorker>(
      new shell_integration::DefaultBrowserWorker(
          base::Bind(&OnCheckIsDefaultBrowserFinished, profile->GetPath(),
                     ShouldShowDefaultBrowserPrompt(profile))))
      ->StartCheckIsDefault();
}

void DefaultBrowserPromptDeclined(Profile* profile) {
  profile->GetPrefs()->SetInt64(prefs::kDefaultBrowserLastDeclined,
                                base::Time::Now().ToInternalValue());
}

void ResetDefaultBrowserPrompt(Profile* profile) {
  profile->GetPrefs()->ClearPref(prefs::kDefaultBrowserLastDeclined);
}

#if !defined(OS_WIN)
bool ShowFirstRunDefaultBrowserPrompt(Profile* profile) {
  return false;
}
#endif

}  // namespace chrome
