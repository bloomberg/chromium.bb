// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_
#define CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

class GlobalError;
class Profile;

class GlobalErrorBubble : public BubbleDelegateGtk,
                          public GlobalErrorBubbleViewBase {
 public:
  GlobalErrorBubble(Browser* browser,
                    const base::WeakPtr<GlobalError>& error,
                    GtkWidget* anchor);
  virtual ~GlobalErrorBubble();

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnAcceptButton);
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnCancelButton);
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnRealize);

  virtual void CloseBubbleView() OVERRIDE;

  Browser* browser_;
  BubbleGtk* bubble_;
  base::WeakPtr<GlobalError> error_;
  std::vector<GtkWidget*> message_labels_;
  int message_width_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubble);
};

#endif  // CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_
