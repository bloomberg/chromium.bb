// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_BADGE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_BADGE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/global_error/global_error.h"

// Non-modal GlobalError implementation that warns the user if extensions
// created warnings or errors. If the user clicks on the wrench menu, the user
// is redirected to chrome://extensions to inspect the errors.
//
// This class is a candidate to be merged with ExtensionGlobalError once
// it is finished.
class ExtensionGlobalErrorBadge : public GlobalError {
 public:
  ExtensionGlobalErrorBadge();
  virtual ~ExtensionGlobalErrorBadge();

  // Implementation for GlobalError:
  virtual bool HasBadge() OVERRIDE;

  virtual bool HasMenuItem() OVERRIDE;
  virtual int MenuItemCommandID() OVERRIDE;
  virtual string16 MenuItemLabel() OVERRIDE;
  virtual void ExecuteMenuItem(Browser* browser) OVERRIDE;

  virtual bool HasBubbleView() OVERRIDE;
  virtual string16 GetBubbleViewTitle() OVERRIDE;
  virtual string16 GetBubbleViewMessage() OVERRIDE;
  virtual string16 GetBubbleViewAcceptButtonLabel() OVERRIDE;
  virtual string16 GetBubbleViewCancelButtonLabel() OVERRIDE;
  virtual void OnBubbleViewDidClose(Browser* browser) OVERRIDE;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) OVERRIDE;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) OVERRIDE;

  static int GetMenuItemCommandID();
 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionGlobalErrorBadge);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_GLOBAL_ERROR_BADGE_H_
