// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_
#define CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"

class Browser;
class GlobalErrorBubbleViewBase;

// This object describes a single global error.
class GlobalError : public base::SupportsWeakPtr<GlobalError> {
 public:
  enum Severity {
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
  };

  GlobalError();
  virtual ~GlobalError();

  // Returns the error's severity level. If there are multiple errors,
  // the error with the highest severity will display in the menu. If not
  // overridden, this is based on the badge resource ID.
  virtual Severity GetSeverity();

  // Returns true if a menu item should be added to the wrench menu.
  virtual bool HasMenuItem() = 0;
  // Returns the command ID for the menu item.
  virtual int MenuItemCommandID() = 0;
  // Returns the label for the menu item.
  virtual string16 MenuItemLabel() = 0;
  // Returns the resource ID for the menu item icon.
  virtual int MenuItemIconResourceID();
  // Called when the user clicks on the menu item.
  virtual void ExecuteMenuItem(Browser* browser) = 0;

  // Returns true if a bubble view should be shown.
  virtual bool HasBubbleView() = 0;
  // Returns true if the bubble view has been shown.
  virtual bool HasShownBubbleView();
  // Called to show the bubble view.
  void ShowBubbleView(Browser* browser);
  // Returns the bubble view.
  virtual GlobalErrorBubbleViewBase* GetBubbleView();
  // Returns the resource ID for bubble view icon.
  virtual int GetBubbleViewIconResourceID();
  // Returns the title for the bubble view.
  virtual string16 GetBubbleViewTitle() = 0;
  // Returns the message for the bubble view.
  virtual string16 GetBubbleViewMessage() = 0;
  // Returns the accept button label for the bubble view.
  virtual string16 GetBubbleViewAcceptButtonLabel() = 0;
  // Returns the cancel button label for the bubble view. To hide the cancel
  // button return an empty string.
  virtual string16 GetBubbleViewCancelButtonLabel() = 0;
  // Called when the bubble view is closed. |browser| is the Browser that the
  // bubble view was shown on.
  void BubbleViewDidClose(Browser* browser);
  // Notifies subclasses that the bubble view is closed. |browser| is the
  // Browser that the bubble view was shown on.
  virtual void OnBubbleViewDidClose(Browser* browser) = 0;
  // Called when the user clicks on the accept button. |browser| is the
  // Browser that the bubble view was shown on.
  virtual void BubbleViewAcceptButtonPressed(Browser* browser) = 0;
  // Called when the user clicks the cancel button. |browser| is the
  // Browser that the bubble view was shown on.
  virtual void BubbleViewCancelButtonPressed(Browser* browser) = 0;


 private:
  bool has_shown_bubble_view_;
  GlobalErrorBubbleViewBase* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(GlobalError);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_ERROR_GLOBAL_ERROR_H_
