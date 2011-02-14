// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_LOCATION_BAR_H_
#define CHROME_TEST_TEST_LOCATION_BAR_H_
#pragma once

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class TestLocationBar : public LocationBar {
 public:
  TestLocationBar();
  virtual ~TestLocationBar();

  void set_input_string(const std::wstring& input_string) {
    input_string_ = input_string;
  }
  void set_disposition(WindowOpenDisposition disposition) {
    disposition_ = disposition;
  }
  void set_transition(PageTransition::Type transition) {
    transition_ = transition;
  }

  // Overridden from LocationBar:
  virtual void ShowFirstRunBubble(FirstRun::BubbleType bubble_type) {}
  virtual void SetSuggestedText(const string16& text) {}
  virtual std::wstring GetInputString() const;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const;
  virtual PageTransition::Type GetPageTransition() const;
  virtual void AcceptInput() {}
  virtual void FocusLocation(bool select_all) {}
  virtual void FocusSearch() {}
  virtual void UpdateContentSettingsIcons() {}
  virtual void UpdatePageActions() {}
  virtual void InvalidatePageActions() {}
  virtual void SaveStateToContents(TabContents* contents) {}
  virtual void Revert() {}
  virtual const AutocompleteEditView* location_entry() const;
  virtual AutocompleteEditView* location_entry();
  virtual LocationBarTesting* GetLocationBarForTesting();

 private:

  // Test-supplied values that will be returned through the LocationBar
  // interface.
  std::wstring input_string_;
  WindowOpenDisposition disposition_;
  PageTransition::Type transition_;

  DISALLOW_COPY_AND_ASSIGN(TestLocationBar);
};


#endif  // CHROME_TEST_TEST_LOCATION_BAR_H_
