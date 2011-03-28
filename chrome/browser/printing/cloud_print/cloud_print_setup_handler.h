// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_HANDLER_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_HANDLER_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"

// Cloud Print setup handler.
// Provides a weak pointer adapter so that callers of
// CloudPrintSetupFlow::OpenDialog can still be notified when the dialog
// completes, but don't have to stick around until the end. Lifetime should be
// shorter than that of it's owner.
class CloudPrintSetupHandler
    : public CloudPrintSetupFlow::Delegate,
      public base::SupportsWeakPtr<CloudPrintSetupHandler> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called when the setup dialog is closed.
    virtual void OnCloudPrintSetupClosed() = 0;
  };

  explicit CloudPrintSetupHandler(Delegate* handler);
  virtual ~CloudPrintSetupHandler();

  // CloudPrintSetupFlow::Delegate implementation.
  virtual void OnDialogClosed();

 private:
  Delegate* handler_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintSetupHandler);
};

// Workaround for MSVC 2005 not handling inheritance from nested classes well.
typedef CloudPrintSetupHandler::Delegate CloudPrintSetupHandlerDelegate;

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_SETUP_HANDLER_H_
