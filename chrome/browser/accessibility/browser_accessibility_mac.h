// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
#pragma once

#include <map>
#include <utility>
#include <vector>

#include "chrome/browser/accessibility/browser_accessibility.h"

@class BrowserAccessibilityCocoa;

class BrowserAccessibilityMac : public BrowserAccessibility {
 public:
  // Implementation of BrowserAccessibility.
  virtual void Initialize();
  virtual void ReleaseReference();

  // Accessers that allow the cocoa wrapper to read these values.
  const string16& name() const { return name_; }
  const string16& value() const { return value_; }
  const std::map<int32, string16>& attributes() const { return attributes_; }

  const std::vector<std::pair<string16, string16> >& html_attributes() const {
    return html_attributes_;
  }

  int32 role() const { return role_; }
  int32 state() const { return state_; }
  const string16& role_name() const { return role_name_; }

  // Accesser and setter for
  // the BrowserAccessibilityCocoa associated with us.
  BrowserAccessibilityCocoa* native_view() const {
    return browser_accessibility_cocoa_;
  }

  void native_view(BrowserAccessibilityCocoa* v) {
    browser_accessibility_cocoa_ = v;
  }

 private:
  // This gives BrowserAccessibility::Create access to the class constructor.
  friend class BrowserAccessibility;

  BrowserAccessibilityMac();

  // Allows access to the BrowserAccessibilityCocoa which wraps this.
  // BrowserAccessibility. We only initialize this member if the accessibility
  // API requests this object.
  BrowserAccessibilityCocoa* browser_accessibility_cocoa_;
  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityMac);
};

#endif // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
