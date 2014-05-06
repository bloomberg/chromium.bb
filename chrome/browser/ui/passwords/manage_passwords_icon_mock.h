// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_MOCK_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_MOCK_H_

#include "chrome/browser/ui/passwords/manage_passwords_icon.h"

// Mock ManagePasswordsIcon, for testing purposes only.
class ManagePasswordsIconMock : public ManagePasswordsIcon {
 public:
  ManagePasswordsIconMock();
  virtual ~ManagePasswordsIconMock();

 protected:
  // ManagePasswordsIcon:
  virtual void UpdateVisibleUI() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsIconMock);
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_ICON_MOCK_H_
