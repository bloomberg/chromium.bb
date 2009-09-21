// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_
#define CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/autocomplete_text_field.h"

@class AutocompleteTextFieldEditor;

// Return the right field editor for AutocompleteTextField instance.

@interface AutocompleteTextFieldWindowTestDelegate : NSObject {
  scoped_nsobject<AutocompleteTextFieldEditor> editor_;
}
- (id)windowWillReturnFieldEditor:(NSWindow *)sender toObject:(id)anObject;
@end

namespace {

// Allow monitoring calls into AutocompleteTextField's observer.

class AutocompleteTextFieldObserverMock : public AutocompleteTextFieldObserver {
 public:
  virtual void OnControlKeyChanged(bool pressed) {
    on_control_key_changed_called_ = true;
    on_control_key_changed_value_ = pressed;
  }

  virtual void OnPaste() {
    on_paste_called_ = true;
  }

  void Reset() {
    on_control_key_changed_called_ = false;
    on_control_key_changed_value_ = false;
    on_paste_called_ = false;
  }

  bool on_control_key_changed_called_;
  bool on_control_key_changed_value_;
  bool on_paste_called_;
};

}  // namespace

#endif  // CHROME_BROWSER_COCOA_AUTOCOMPLETE_TEXT_FIELD_UNITTEST_HELPER_H_
