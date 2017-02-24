// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_

#include "base/feature_list.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class Profile;

namespace chrome {

// The StickyDefaultBrowserPrompt experiment delays the dismissal of the
// DefaultBrowserPrompt to after a change in the default browser setting has
// been detected.
constexpr base::Feature kStickyDefaultBrowserPrompt{
    "StickyDefaultBrowserPrompt", base::FEATURE_DISABLED_BY_DEFAULT};

// Returns true if the StickyDefaultBrowserPrompt experiment is enabled.
bool IsStickyDefaultBrowserPromptEnabled();

// The delegate for the infobar shown when Chrome is not the default browser.
// Ownership of the delegate is given to the infobar itself, the lifetime of
// which is bound to the containing WebContents.
class DefaultBrowserInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a default browser infobar and delegate and adds the infobar to
  // |infobar_service|.
  static void Create(InfoBarService* infobar_service, Profile* profile);

 protected:
  explicit DefaultBrowserInfoBarDelegate(Profile* profile);
  ~DefaultBrowserInfoBarDelegate() override;

 private:
  // Possible user interactions with the default browser info bar.
  // Do not modify the ordering as it is important for UMA.
  enum InfoBarUserInteraction {
    // The user clicked the "OK" (i.e., "Set as default") button.
    ACCEPT_INFO_BAR = 0,
    // The cancel button is deprecated.
    // CANCEL_INFO_BAR = 1,
    // The user did not interact with the info bar.
    IGNORE_INFO_BAR = 2,
    // The user explicitly closed the infobar.
    DISMISS_INFO_BAR = 3,
    NUM_INFO_BAR_USER_INTERACTION_TYPES
  };

  void AllowExpiry();

  // ConfirmInfoBarDelegate:
  Type GetInfoBarType() const override;
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool OKButtonTriggersUACPrompt() const override;
  bool Accept() override;

  // Declared virtual so tests can override.
  virtual scoped_refptr<shell_integration::DefaultBrowserWorker>
  CreateDefaultBrowserWorker(
      const shell_integration::DefaultWebClientWorkerCallback& callback);

  // Removes the infobar when the worker is finished setting the default
  // browser. Only used if the StickyDefaultBrowserPrompt experiment is
  // enabled.
  void OnSetAsDefaultFinished(shell_integration::DefaultWebClientState state);

  // The WebContents's corresponding profile.
  Profile* profile_;

  scoped_refptr<base::SingleThreadTaskRunner> thread_task_runner_;

  // Whether the info bar should be dismissed on the next navigation.
  bool should_expire_;

  // Indicates if the user interacted with the infobar.
  bool action_taken_;

  // Used to delay the expiration of the info-bar.
  base::WeakPtrFactory<DefaultBrowserInfoBarDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultBrowserInfoBarDelegate);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_INFOBAR_DELEGATE_H_
