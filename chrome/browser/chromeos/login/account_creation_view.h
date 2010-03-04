// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

#include <string>

#include "chrome/browser/views/dom_view.h"

class Profile;
class SiteContents;
class TabContentsDelegate;

class AccountCreationView : public DOMView {
 public:
  AccountCreationView();
  virtual ~AccountCreationView();

  void Init();
  void InitDOM(Profile* profile, SiteInstance* site_instance);
  void SetTabContentsDelegate(TabContentsDelegate* delegate);

 private:
  // Overriden from views::View:
  virtual void Paint(gfx::Canvas* canvas);

  DISALLOW_COPY_AND_ASSIGN(AccountCreationView);
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACCOUNT_CREATION_VIEW_H_

