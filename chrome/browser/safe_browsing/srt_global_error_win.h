// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error.h"

class GlobalErrorBubbleViewBase;
class GlobalErrorService;

// Encapsulates UI-related functionality for the software removal tool (SRT)
// prompt. The UI consists of two parts: (1.) the profile reset (pop-up) bubble,
// and (2.) a menu item in the wrench menu (provided by being a GlobalError).
class SRTGlobalError : public GlobalErrorWithStandardBubble,
                       public base::SupportsWeakPtr<SRTGlobalError> {
 public:
  explicit SRTGlobalError(GlobalErrorService* global_error_service);
  virtual ~SRTGlobalError();

  // GlobalError:
  virtual bool HasMenuItem() override;
  virtual int MenuItemCommandID() override;
  virtual base::string16 MenuItemLabel() override;
  virtual void ExecuteMenuItem(Browser* browser) override;
  virtual void ShowBubbleView(Browser* browser) override;

  // GlobalErrorWithStandardBubble.
  virtual base::string16 GetBubbleViewTitle() override;
  virtual std::vector<base::string16> GetBubbleViewMessages() override;
  virtual base::string16 GetBubbleViewAcceptButtonLabel() override;
  virtual base::string16 GetBubbleViewCancelButtonLabel() override;
  virtual void OnBubbleViewDidClose(Browser* browser) override;
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) override;
  virtual void BubbleViewCancelButtonPressed(Browser* browser) override;
  virtual bool ShouldCloseOnDeactivate() const override;

 private:
  // When the user took action, the GlobalError can be dismissed.
  void DismissGlobalError();

  // Used to dismiss the GlobalError, then set to NULL.
  GlobalErrorService* global_error_service_;

  DISALLOW_COPY_AND_ASSIGN(SRTGlobalError);
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_GLOBAL_ERROR_WIN_H_
