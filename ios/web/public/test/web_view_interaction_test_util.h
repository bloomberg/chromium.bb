// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state.h"

#include <string>

namespace web {
namespace test {

// Returns whether the element with |element_id| in the passed |web_state| has
// been tapped using a JavaScript click() event.
bool TapWebViewElementWithId(web::WebState* web_state,
                             const std::string& element_id);

// Returns whether the element with |element_id| in the passed |web_state| has
// been focused using a JavaScript focus() event.
bool FocusWebViewElementWithId(web::WebState* web_state,
                               const std::string& element_id);

// Returns whether the form with |form_id| in the passed |web_state| has been
// submitted using a JavaScript submit() event.
bool SubmitWebViewFormWithId(web::WebState* web_state,
                             const std::string& form_id);
}  // namespace test
}  // namespace web
