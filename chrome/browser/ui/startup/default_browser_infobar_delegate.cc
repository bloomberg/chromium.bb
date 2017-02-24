// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/startup/default_browser_prompt.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace chrome {

bool IsStickyDefaultBrowserPromptEnabled() {
#if defined(OS_WIN)
  // The flow to set the default browser is only asynchronous on Windows 10+.
  return base::win::GetVersion() >= base::win::VERSION_WIN10 &&
         base::FeatureList::IsEnabled(kStickyDefaultBrowserPrompt);
#else
  return false;
#endif
}

// static
void DefaultBrowserInfoBarDelegate::Create(InfoBarService* infobar_service,
                                           Profile* profile) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new DefaultBrowserInfoBarDelegate(profile))));
}

DefaultBrowserInfoBarDelegate::DefaultBrowserInfoBarDelegate(Profile* profile)
    : ConfirmInfoBarDelegate(),
      profile_(profile),
      should_expire_(false),
      action_taken_(false),
      weak_factory_(this) {
  // We want the info-bar to stick-around for few seconds and then be hidden
  // on the next navigation after that.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&DefaultBrowserInfoBarDelegate::AllowExpiry,
                            weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(8));
}

DefaultBrowserInfoBarDelegate::~DefaultBrowserInfoBarDelegate() {
  if (!action_taken_) {
    content::RecordAction(
        base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
    UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                              IGNORE_INFO_BAR,
                              NUM_INFO_BAR_USER_INTERACTION_TYPES);
  }
}

void DefaultBrowserInfoBarDelegate::AllowExpiry() {
  should_expire_ = true;
}

infobars::InfoBarDelegate::Type DefaultBrowserInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

infobars::InfoBarDelegate::InfoBarIdentifier
DefaultBrowserInfoBarDelegate::GetIdentifier() const {
  return DEFAULT_BROWSER_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& DefaultBrowserInfoBarDelegate::GetVectorIcon() const {
  return kProductIcon;
}

bool DefaultBrowserInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return should_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

void DefaultBrowserInfoBarDelegate::InfoBarDismissed() {
  action_taken_ = true;
  // |profile_| may be null in tests.
  if (profile_)
    chrome::DefaultBrowserPromptDeclined(profile_);
  content::RecordAction(
      base::UserMetricsAction("DefaultBrowserInfoBar_Dismiss"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            DISMISS_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
}

base::string16 DefaultBrowserInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_SHORT_TEXT);
}

int DefaultBrowserInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 DefaultBrowserInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL);
}

// Setting an app as the default browser doesn't require elevation directly, but
// it does require registering it as the protocol handler for "http", so if
// protocol registration in general requires elevation, this does as well.
bool DefaultBrowserInfoBarDelegate::OKButtonTriggersUACPrompt() const {
  return shell_integration::IsElevationNeededForSettingDefaultProtocolClient();
}

bool DefaultBrowserInfoBarDelegate::Accept() {
  action_taken_ = true;
  content::RecordAction(
      base::UserMetricsAction("DefaultBrowserInfoBar_Accept"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            ACCEPT_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);

  bool close_infobar = true;
  shell_integration::DefaultWebClientWorkerCallback set_as_default_callback;

  if (IsStickyDefaultBrowserPromptEnabled()) {
    // When the experiment is enabled, the infobar is only closed when the
    // DefaultBrowserWorker is finished.
    set_as_default_callback =
        base::Bind(&DefaultBrowserInfoBarDelegate::OnSetAsDefaultFinished,
                   weak_factory_.GetWeakPtr());
    close_infobar = false;
  }

  // The worker pointer is reference counted. While it is running, the
  // message loops of the FILE and UI thread will hold references to it
  // and it will be automatically freed once all its tasks have finished.
  CreateDefaultBrowserWorker(set_as_default_callback)->StartSetAsDefault();
  return close_infobar;
}

scoped_refptr<shell_integration::DefaultBrowserWorker>
DefaultBrowserInfoBarDelegate::CreateDefaultBrowserWorker(
    const shell_integration::DefaultWebClientWorkerCallback& callback) {
  return new shell_integration::DefaultBrowserWorker(callback);
}

void DefaultBrowserInfoBarDelegate::OnSetAsDefaultFinished(
    shell_integration::DefaultWebClientState state) {
  infobar()->owner()->RemoveInfoBar(infobar());
}

}  // namespace chrome
