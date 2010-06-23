// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_modal_dialog.h"

#include "base/logging.h"
#include "chrome/browser/views/jsmessage_box_dialog.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

AppModalDialog::~AppModalDialog() {
}

void AppModalDialog::CreateAndShowDialog() {
  dialog_ = CreateNativeDialog();
  DCHECK(dialog_->IsModal());
  dialog_->ShowModalDialog();
}

void AppModalDialog::ActivateModalDialog() {
  DCHECK(dialog_);
  dialog_->ActivateModalDialog();
}

void AppModalDialog::CloseModalDialog() {
  DCHECK(dialog_);
  dialog_->CloseModalDialog();
}
