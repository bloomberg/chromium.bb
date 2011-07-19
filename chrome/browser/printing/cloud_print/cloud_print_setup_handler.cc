// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/cloud_print/cloud_print_setup_handler.h"

CloudPrintSetupHandler::CloudPrintSetupHandler(Delegate* handler)
    : handler_(handler) {}

CloudPrintSetupHandler::~CloudPrintSetupHandler() {}

void CloudPrintSetupHandler::OnDialogClosed() {
  DCHECK(handler_);
  handler_->OnCloudPrintSetupClosed();
}
