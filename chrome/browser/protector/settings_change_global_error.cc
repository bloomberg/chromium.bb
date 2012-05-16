// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/settings_change_global_error.h"

#include <bitset>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/settings_change_global_error_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace protector {

namespace {

// Timeout before the global error is removed (wrench menu item disappears).
const int kMenuItemDisplayPeriodMs = 10*60*1000;  // 10 min

// Unset bits indicate available command IDs.
static base::LazyInstance<
    std::bitset<IDC_SHOW_SETTINGS_CHANGE_LAST -
                IDC_SHOW_SETTINGS_CHANGE_FIRST + 1> > menu_ids =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

SettingsChangeGlobalError::SettingsChangeGlobalError(
    BaseSettingChange* change,
    SettingsChangeGlobalErrorDelegate* delegate)
    : change_(change),
      delegate_(delegate),
      profile_(NULL),
      closed_by_button_(false),
      show_on_browser_activation_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)),
      menu_id_(-1) {
  DCHECK(delegate_);
  for (int i = IDC_SHOW_SETTINGS_CHANGE_FIRST;
       i <= IDC_SHOW_SETTINGS_CHANGE_LAST; i++) {
    if (!menu_ids.Get()[i - IDC_SHOW_SETTINGS_CHANGE_FIRST]) {
      menu_id_ = i;
      menu_ids.Get().set(i - IDC_SHOW_SETTINGS_CHANGE_FIRST);
      break;
    }
  }
  DCHECK(menu_id_ >= 0) << "Out of command IDs for SettingsChangeGlobalError";
}

SettingsChangeGlobalError::~SettingsChangeGlobalError() {
  if (profile_)
    RemoveFromProfile();
  if (menu_id_ >= 0)
    menu_ids.Get().reset(menu_id_ - IDC_SHOW_SETTINGS_CHANGE_FIRST);
}

void SettingsChangeGlobalError::AddToProfile(Profile* profile,
                                             bool show_bubble) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  profile_ = profile;
  GlobalErrorServiceFactory::GetForProfile(profile_)->AddGlobalError(this);
  BrowserList::AddObserver(this);
  if (show_bubble) {
    ShowBubble();
  } else {
    // Start inactivity timer.
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::OnInactiveTimeout,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kMenuItemDisplayPeriodMs));
  }
}

void SettingsChangeGlobalError::RemoveFromProfile() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile_) {
    GlobalErrorServiceFactory::GetForProfile(profile_)->RemoveGlobalError(this);
    profile_ = NULL;
  }
  BrowserList::RemoveObserver(this);
  // This will delete |this|.
  delegate_->OnRemovedFromProfile(this);
}

void SettingsChangeGlobalError::ShowBubble() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  Browser* browser = browser::FindTabbedBrowser(
      profile_,
      // match incognito
      true);
  if (browser)
    ShowBubbleInBrowser(browser);
}

bool SettingsChangeGlobalError::HasBadge() {
  return true;
}

int SettingsChangeGlobalError::GetBadgeResourceID() {
  return change_->GetBadgeIconID();
}

bool SettingsChangeGlobalError::HasMenuItem() {
  return true;
}

int SettingsChangeGlobalError::MenuItemCommandID() {
  return menu_id_;
}

string16 SettingsChangeGlobalError::MenuItemLabel() {
  return change_->GetBubbleTitle();
}

int SettingsChangeGlobalError::MenuItemIconResourceID() {
  return change_->GetMenuItemIconID();
}

void SettingsChangeGlobalError::ExecuteMenuItem(Browser* browser) {
  ShowBubbleInBrowser(browser);
}

bool SettingsChangeGlobalError::HasBubbleView() {
  return true;
}

int SettingsChangeGlobalError::GetBubbleViewIconResourceID() {
  return change_->GetBubbleIconID();
}

string16 SettingsChangeGlobalError::GetBubbleViewTitle() {
  return change_->GetBubbleTitle();
}

string16 SettingsChangeGlobalError::GetBubbleViewMessage() {
  return change_->GetBubbleMessage();
}

// The Accept and Revert buttons are swapped like the 'server' and 'client'
// concepts in X11. Accept button (the default one) discards changes
// (keeps using previous setting) while cancel button applies changes
// (switches to the new setting). This is sick and blows my mind. - ivankr

string16 SettingsChangeGlobalError::GetBubbleViewAcceptButtonLabel() {
  return change_->GetDiscardButtonText();
}

string16 SettingsChangeGlobalError::GetBubbleViewCancelButtonLabel() {
  return change_->GetApplyButtonText();
}

void SettingsChangeGlobalError::OnBubbleViewDidClose(Browser* browser) {
  // The bubble may be closed as the result of RemoveFromProfile call when
  // merging this error with another one.
  if (!profile_)
    return;
  if (!closed_by_button_) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::OnInactiveTimeout,
                   weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kMenuItemDisplayPeriodMs));
#if !defined(TOOLKIT_GTK)
    // TODO(ivankr): the logic for redisplaying bubble is disabled on Gtk, see
    // http://crbug.com/115719.
    if (browser->window() &&
        !platform_util::IsWindowActive(browser->window()->GetNativeHandle())) {
      // Bubble closed because the entire window lost activation, display
      // again when a window gets active.
      show_on_browser_activation_ = true;
    }
#endif
  } else {
    RemoveFromProfile();
  }
}

void SettingsChangeGlobalError::BubbleViewAcceptButtonPressed(
    Browser* browser) {
  closed_by_button_ = true;
  delegate_->OnDiscardChange(this, browser);
}

void SettingsChangeGlobalError::BubbleViewCancelButtonPressed(
    Browser* browser) {
  closed_by_button_ = true;
  delegate_->OnApplyChange(this, browser);
}

void SettingsChangeGlobalError::OnBrowserSetLastActive(
    const Browser* browser) {
  if (show_on_browser_activation_ && browser && browser->is_type_tabbed()) {
    // A tabbed browser window got activated, show the error bubble again.
    // Calling ShowBubble() immediately from here does not always work because
    // the old browser window may still have focus.
    // Multiple posted ShowBubble() calls are fine since the first successful
    // one will invalidate all the weak pointers.
    // Note that ShowBubble() will display the bubble in the last active browser
    // (which may not be |browser| at the moment ShowBubble() is executed).
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SettingsChangeGlobalError::ShowBubble,
                   weak_factory_.GetWeakPtr()));
  }
}

void SettingsChangeGlobalError::ShowBubbleInBrowser(Browser* browser) {
  show_on_browser_activation_ = false;
  // Cancel any previously posted tasks so that the global error
  // does not get removed on timeout while still showing the bubble.
  weak_factory_.InvalidateWeakPtrs();
  ShowBubbleView(browser);
}

void SettingsChangeGlobalError::OnInactiveTimeout() {
  delegate_->OnDecisionTimeout(this);
  RemoveFromProfile();
}

}  // namespace protector
