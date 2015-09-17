// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

// TODO(tapted): Delete this file when Mac uses the password bubble by default.

scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  return make_scoped_ptr(new ConfirmInfoBar(delegate.Pass()));
}
