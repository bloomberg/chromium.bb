// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
#define CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
#pragma once

#include "base/lazy_instance.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"

class ListValue;
class TabContents;

namespace chromeos {

class SystemNotification;

class LocaleChangeGuard {
 public:
  // When called first time for user profile: performs check whether
  // locale has been changed automatically recently (based on synchronized user
  // preference).  If so: shows notification that allows user to revert change.
  // On subsequent calls: does nothing (hopefully fast).
  static void Check(TabContents* tab_contents);

 private:
  class Delegate;

  LocaleChangeGuard();
  void CheckLocaleChange(TabContents* tab_contents);
  void RevertLocaleChange(const ListValue* list);
  void AcceptLocaleChange();

  std::string from_locale_;
  std::string to_locale_;
  ProfileId profile_id_;
  TabContents* tab_contents_;
  scoped_ptr<chromeos::SystemNotification> note_;
  bool reverted_;

  friend struct base::DefaultLazyInstanceTraits<LocaleChangeGuard>;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOCALE_CHANGE_GUARD_H_
