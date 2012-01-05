// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "ui/gfx/rect.h"

namespace content {
class WebContents;
}

class AutofillPopupView : public content::NotificationObserver {
 public:
  explicit AutofillPopupView(content::WebContents* web_contents);
  virtual ~AutofillPopupView();

  // Hide the popup from view.
  virtual void Hide() = 0;

  // Display the autofill popup and fill it in with the values passed in.
  virtual void Show(const std::vector<string16>& autofill_values,
                    const std::vector<string16>& autofill_labels,
                    const std::vector<string16>& autofill_icons,
                    const std::vector<int>& autofill_unique_ids,
                    int separator_index) = 0;


  void set_element_bounds(const gfx::Rect& bounds) {
    element_bounds_ = bounds;
  }

  const gfx::Rect& element_bounds() { return element_bounds_; }

 private:
  // content::NotificationObserver method override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  // The bounds of the text element that is the focus of the Autofill.
  gfx::Rect element_bounds_;
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_POPUP_VIEW_H_
