// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_error_ui.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/common/extensions/extension.h"

class Browser;
class ExtensionService;

class ExtensionErrorUIDefault : public ExtensionErrorUI {
 public:
  explicit ExtensionErrorUIDefault(ExtensionService* extension_service);
  virtual ~ExtensionErrorUIDefault();

  // ExtensionErrorUI implementation:
  virtual bool ShowErrorInBubbleView() OVERRIDE;
  virtual void ShowExtensions() OVERRIDE;

 private:
  class ExtensionGlobalError : public GlobalError {
   public:
    explicit ExtensionGlobalError(ExtensionErrorUIDefault* error_ui);

   private:
    // GlobalError methods.
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

    // The ExtensionErrorUIDefault who owns us.
    ExtensionErrorUIDefault* error_ui_;

    DISALLOW_COPY_AND_ASSIGN(ExtensionGlobalError);
  };

  // The browser the bubble view was shown into.
  Browser* browser_;

  scoped_ptr<ExtensionGlobalError> global_error_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionErrorUIDefault);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ERROR_UI_DEFAULT_H_
