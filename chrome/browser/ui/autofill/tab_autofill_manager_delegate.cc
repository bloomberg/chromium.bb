// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

TabAutofillManagerDelegate::TabAutofillManagerDelegate(TabContents* tab)
    : tab_(tab) {
  DCHECK(tab_);
}

InfoBarTabService* TabAutofillManagerDelegate::GetInfoBarService() {
  return tab_->infobar_tab_helper();
}

PrefServiceBase* TabAutofillManagerDelegate::GetPrefs() {
  return tab_->profile()->GetPrefs();
}

bool TabAutofillManagerDelegate::IsSavingPasswordsEnabled() const {
  return tab_->password_manager()->IsSavingEnabled();
}
