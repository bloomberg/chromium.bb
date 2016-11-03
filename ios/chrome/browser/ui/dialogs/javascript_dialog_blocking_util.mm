// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/dialogs/javascript_dialog_blocking_util.h"

#import <objc/runtime.h>

#include "base/logging.h"
#include "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The key under which JavaScriptDialogBlockingStates are associated with
// their WebStates.
const void* const kBlockingStateKey = &kBlockingStateKey;
}

#pragma mark - JavaScriptDialogBlockingStateWrapper

class JavaScriptDialogBlockingState;

// Wrapper class used to associated JavaScriptDialogBlockingStates with their
// WebStates.
class JavaScriptDialogBlockingStateWrapper
    : public base::SupportsUserData::Data {
 public:
  // Factory method.  Guaranteed to return a non-null value.
  static JavaScriptDialogBlockingStateWrapper* FromWebState(
      web::WebState* web_state) {
    DCHECK(web_state);
    JavaScriptDialogBlockingStateWrapper* wrapper =
        static_cast<JavaScriptDialogBlockingStateWrapper*>(
            web_state->GetUserData(kBlockingStateKey));
    if (!wrapper)
      wrapper = new JavaScriptDialogBlockingStateWrapper(web_state);
    return wrapper;
  }

  // The JavaScriptDialogBlockingState owned by this wrapper.
  JavaScriptDialogBlockingState* state() const { return state_.get(); }
  void set_state(JavaScriptDialogBlockingState* state) { state_.reset(state); }

 private:
  std::unique_ptr<JavaScriptDialogBlockingState> state_;

  // Private constructor associating the newly created object with |web_state|
  // via its SupportsUserData.
  explicit JavaScriptDialogBlockingStateWrapper(web::WebState* web_state)
      : state_(nullptr) {
    DCHECK(web_state);
    web_state->SetUserData(kBlockingStateKey, this);
  }
};

#pragma mark - JavaScriptDialogBlockingState

// Wrapper class used to associated JavaScriptDialogBlockingStates with their
// WebStates.
class JavaScriptDialogBlockingState : public web::WebStateObserver {
 public:
  // Factory method.  Guaranteed to return a non-null value.
  static JavaScriptDialogBlockingState* FromWebState(web::WebState* web_state) {
    DCHECK(web_state);
    JavaScriptDialogBlockingStateWrapper* wrapper =
        JavaScriptDialogBlockingStateWrapper::FromWebState(web_state);
    if (!wrapper->state())
      wrapper->set_state(new JavaScriptDialogBlockingState(web_state));
    return wrapper->state();
  }

  // Whether JavaScript dialogs should be blocked for the WebState.
  bool ShouldBlockDialogs() const { return should_block_dialogs_; }
  void SetShouldBlockDialogs(bool should_block_dialogs) {
    should_block_dialogs_ = should_block_dialogs;
  }

  // Whether the blocking option should be shown for the next JavaScript dialog
  // presented by the WebState.
  bool ShouldShowBlockingOption() const { return dialog_count_ >= 1; }

  // Called whenever a JavaScript dialog is displayed by the WebState.
  void DialogWasShown() { dialog_count_++; }

  // WebStateObserver overrides:
  void NavigationItemCommitted(
      const web::LoadCommittedDetails& load_details) override {
    // The blocking state should be reset after each page load.
    dialog_count_ = 0;
    should_block_dialogs_ = false;
  }

  void WebStateDestroyed() override {
    // Stop observing the WebState when it's destroyed since this object will be
    // deleted after its WebState's destructor.  This avoids calling
    // RemoveObserver() on a deallocated WebState from the WebStateObserver
    // destructor.
    Observe(nullptr);
  }

 protected:
  // Constructor that begins observing |web_state|.
  explicit JavaScriptDialogBlockingState(web::WebState* web_state)
      : web::WebStateObserver(web_state),
        dialog_count_(0),
        should_block_dialogs_(false) {}

 private:
  size_t dialog_count_;
  bool should_block_dialogs_;
};

#pragma mark - javascript_dialog_blocking

bool ShouldShowDialogBlockingOption(web::WebState* web_state) {
  DCHECK(web_state);
  return JavaScriptDialogBlockingState::FromWebState(web_state)
      ->ShouldShowBlockingOption();
}

void JavaScriptDialogWasShown(web::WebState* web_state) {
  DCHECK(web_state);
  JavaScriptDialogBlockingState::FromWebState(web_state)->DialogWasShown();
}

bool ShouldBlockJavaScriptDialogs(web::WebState* web_state) {
  DCHECK(web_state);
  return JavaScriptDialogBlockingState::FromWebState(web_state)
      ->ShouldBlockDialogs();
}

void DialogBlockingOptionSelected(web::WebState* web_state) {
  DCHECK(web_state);
  JavaScriptDialogBlockingState::FromWebState(web_state)->SetShouldBlockDialogs(
      true);
}
