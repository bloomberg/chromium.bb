// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt.h"

#include <string>

#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icons_public.h"

namespace {

// A ShellIntegration::DefaultWebClientObserver that records user metrics for
// the result of making Chrome the default browser.
class SetDefaultBrowserObserver
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  SetDefaultBrowserObserver();
  ~SetDefaultBrowserObserver() override;

 private:
  void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) override;
  void OnSetAsDefaultConcluded(bool succeeded) override;
  bool IsOwnedByWorker() override;
  bool IsInteractiveSetDefaultPermitted() override;

  // True if an interactive flow will be used (i.e., Windows 8+).
  bool interactive_;

  // The result of the call to ShellIntegration::SetAsDefaultBrowser() or
  // ShellIntegration::SetAsDefaultBrowserInteractive().
  bool interaction_succeeded_ = false;

  DISALLOW_COPY_AND_ASSIGN(SetDefaultBrowserObserver);
};

SetDefaultBrowserObserver::SetDefaultBrowserObserver()
    : interactive_(ShellIntegration::CanSetAsDefaultBrowser() ==
                   ShellIntegration::SET_DEFAULT_INTERACTIVE) {
  // Log that an attempt is about to be made to set one way or the other.
  if (interactive_)
    UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.SetAsDefaultUI", true);
  else
    UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.SetAsDefault", true);
}

SetDefaultBrowserObserver::~SetDefaultBrowserObserver() {}

void SetDefaultBrowserObserver::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  if (interactive_ && interaction_succeeded_ &&
      state == ShellIntegration::STATE_NOT_DEFAULT) {
    // The interactive flow succeeded, yet Chrome is not the default browser.
    // This likely means that the user selected another browser from the panel.
    // Consider this the same as canceling the infobar.
    UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.DontSetAsDefault", true);
  }
}

void SetDefaultBrowserObserver::OnSetAsDefaultConcluded(bool succeeded) {
  interaction_succeeded_ = succeeded;
  if (interactive_ && !succeeded) {
    // Log that the interactive flow failed.
    UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.SetAsDefaultUIFailed", true);
  }
}

bool SetDefaultBrowserObserver::IsOwnedByWorker() {
  // Instruct the DefaultBrowserWorker to delete this instance when it is done.
  return true;
}

bool SetDefaultBrowserObserver::IsInteractiveSetDefaultPermitted() {
  return true;
}

// The delegate for the infobar shown when Chrome is not the default browser.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a default browser infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service, PrefService* prefs);

 private:
  explicit DefaultBrowserInfoBarDelegate(PrefService* prefs);
  ~DefaultBrowserInfoBarDelegate() override;

  void AllowExpiry() { should_expire_ = true; }

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  int GetIconId() const override;
  gfx::VectorIconId GetVectorIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  base::string16 GetMessageText() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool OKButtonTriggersUACPrompt() const override;
  bool Accept() override;
  bool Cancel() override;

  // The prefs to use.
  PrefService* prefs_;

  // Whether the user clicked one of the buttons.
  bool action_taken_;

  // Whether the info-bar should be dismissed on the next navigation.
  bool should_expire_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<DefaultBrowserInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegate);
};

// static
void DefaultBrowserInfoBarDelegate::Create(InfoBarService* infobar_service,
                                           PrefService* prefs) {
  infobar_service->AddInfoBar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new DefaultBrowserInfoBarDelegate(prefs))));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(PrefService* prefs)
    : ConfirmInfoBarDelegate(),
      prefs_(prefs),
      action_taken_(false),
      should_expire_(false),
      weak_factory_(this) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&DefaultBrowserInfoBarDelegate::AllowExpiry,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
  if (!action_taken_)
    UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.Ignored", true);
}

infobars::InfoBarDelegate::Type DefaultBrowserInfoBarDelegate::GetInfoBarType()
    const {
#if defined(OS_WIN)
  return WARNING_TYPE;
#else
  return PAGE_ACTION_TYPE;
#endif
}

int DefaultBrowserInfoBarDelegate::GetIconId() const {
  return IDR_PRODUCT_LOGO_32;
}

gfx::VectorIconId DefaultBrowserInfoBarDelegate::GetVectorIconId() const {
#if defined(OS_MACOSX) || defined(OS_ANDROID) || defined(OS_IOS)
  return gfx::VectorIconId::VECTOR_ICON_NONE;
#else
  return gfx::VectorIconId::CHROME_PRODUCT;
#endif
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return should_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

base::string16 DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
}

base::string16 DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_SET_AS_DEFAULT_INFOBAR_BUTTON_LABEL :
      IDS_DONT_ASK_AGAIN_INFOBAR_BUTTON_LABEL);
}

// Setting an app as the default browser doesn't require elevation directly, but
// it does require registering it as the protocol handler for "http", so if
// protocol registration in general requires elevation, this does as well.
bool DefaultBrowserInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return ShellIntegration::IsElevationNeededForSettingDefaultProtocolClient();
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  action_taken_ = true;
  scoped_refptr<ShellIntegration::DefaultBrowserWorker>(
      new ShellIntegration::DefaultBrowserWorker(new SetDefaultBrowserObserver))
      ->StartSetAsDefault();
  return true;
}

bool DefaultBrowserInfoBarDelegate::Cancel() {
  action_taken_ = true;
  UMA_HISTOGRAM_BOOLEAN("DefaultBrowserWarning.DontSetAsDefault", true);
  // User clicked "Don't ask me again", remember that.
  prefs_->SetBoolean(prefs::kCheckDefaultBrowser, false);
  return true;
}

// A ShellIntegration::DefaultWebClientObserver that handles the check to
// determine whether or not to show the default browser prompt. If Chrome is the
// default browser, then the kCheckDefaultBrowser pref is reset.  Otherwise, the
// prompt is shown.
class CheckDefaultBrowserObserver
    : public ShellIntegration::DefaultWebClientObserver {
 public:
  CheckDefaultBrowserObserver(const base::FilePath& profile_path,
                              bool show_prompt,
                              chrome::HostDesktopType desktop_type);
  ~CheckDefaultBrowserObserver() override;

 private:
  void SetDefaultWebClientUIState(
      ShellIntegration::DefaultWebClientUIState state) override;
  bool IsOwnedByWorker() override;

  void ResetCheckDefaultBrowserPref();
  void ShowPrompt();

  // The path to the profile for which the prompt is to be shown.
  base::FilePath profile_path_;

  // True if the prompt is to be shown if Chrome is not the default browser.
  bool show_prompt_;
  chrome::HostDesktopType desktop_type_;

  DISALLOW_COPY_AND_ASSIGN(CheckDefaultBrowserObserver);
};

CheckDefaultBrowserObserver::CheckDefaultBrowserObserver(
    const base::FilePath& profile_path,
    bool show_prompt,
    chrome::HostDesktopType desktop_type)
    : profile_path_(profile_path),
      show_prompt_(show_prompt),
      desktop_type_(desktop_type) {}

CheckDefaultBrowserObserver::~CheckDefaultBrowserObserver() {}

void CheckDefaultBrowserObserver::SetDefaultWebClientUIState(
    ShellIntegration::DefaultWebClientUIState state) {
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    // Notify the user in the future if Chrome ceases to be the user's chosen
    // default browser.
    ResetCheckDefaultBrowserPref();
  } else if (show_prompt_ && state == ShellIntegration::STATE_NOT_DEFAULT &&
             ShellIntegration::CanSetAsDefaultBrowser() !=
                 ShellIntegration::SET_DEFAULT_NOT_ALLOWED) {
    ShowPrompt();
  }
}

bool CheckDefaultBrowserObserver::IsOwnedByWorker() {
  // Instruct the DefaultBrowserWorker to delete this instance when it is done.
  return true;
}

void CheckDefaultBrowserObserver::ResetCheckDefaultBrowserPref() {
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path_);
  if (profile)
    profile->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, true);
}

void CheckDefaultBrowserObserver::ShowPrompt() {
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(desktop_type_);
  if (!browser)
    return;  // Reached during ui tests.

  // In ChromeBot tests, there might be a race. This line appears to get
  // called during shutdown and |tab| can be NULL.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  DefaultBrowserInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->GetPrefs());
}

}  // namespace

namespace chrome {

void RegisterDefaultBrowserPromptPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(
      prefs::kBrowserSuppressDefaultBrowserPrompt, std::string());
}

void ShowDefaultBrowserPrompt(Profile* profile, HostDesktopType desktop_type) {
  // Do not check if Chrome is the default browser if there is a policy in
  // control of this setting.
  if (g_browser_process->local_state()->IsManagedPreference(
      prefs::kDefaultBrowserSettingEnabled)) {
    // Handling of the browser.default_browser_setting_enabled policy setting is
    // taken care of in BrowserProcessImpl.
    return;
  }

  // Check if Chrome is the default browser but do not prompt if:
  // - The user said "don't ask me again" on the infobar earlier.
  // - The "suppress_default_browser_prompt_for_version" master preference is
  //     set to the current version.
  bool show_prompt =
      profile->GetPrefs()->GetBoolean(prefs::kCheckDefaultBrowser);
  if (show_prompt) {
    const std::string disable_version_string =
        g_browser_process->local_state()->GetString(
            prefs::kBrowserSuppressDefaultBrowserPrompt);
    const Version disable_version(disable_version_string);
    DCHECK(disable_version_string.empty() || disable_version.IsValid());
    if (disable_version.IsValid()) {
      if (disable_version.Equals(Version(version_info::GetVersionNumber())))
        show_prompt = false;
    }
  }

  scoped_refptr<ShellIntegration::DefaultBrowserWorker>(
      new ShellIntegration::DefaultBrowserWorker(
          new CheckDefaultBrowserObserver(profile->GetPath(), show_prompt,
                                          desktop_type)))
      ->StartCheckIsDefault();
}

#if !defined(OS_WIN)
bool ShowFirstRunDefaultBrowserPrompt(Profile* profile) {
  return false;
}
#endif

}  // namespace chrome
