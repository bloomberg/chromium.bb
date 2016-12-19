// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_H_
#define IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#import "ios/chrome/browser/autofill/form_suggestion_provider.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"

namespace autofill {
class AutofillPopupDelegate;
struct FormData;
class FormStructure;
}

namespace ios {
class ChromeBrowserState;
}

namespace web {
class WebState;
}

// Handles autofill form suggestions. Reads forms from the page, sends them to
// AutofillManager for metrics and to retrieve suggestions, and fills forms in
// response to user interaction with suggestions. This is the iOS counterpart
// to the upstream class autofill::AutofillAgent.
@interface AutofillAgent : NSObject<FormSuggestionProvider>

@property(nonatomic, readonly) ios::ChromeBrowserState* browserState;

// Designated initializer. Arguments |browserState| and |webState| should not be
// null.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Callback by AutofillController when suggestions are ready.
- (void)onSuggestionsReady:(NSArray*)suggestions
             popupDelegate:
                 (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                     delegate;

// The supplied data should be filled into the form.
- (void)onFormDataFilled:(const autofill::FormData&)result;

// Detatches from the web state.
- (void)detachFromWebState;

// Renders the field type predictions specified in |forms|. This method is a
// no-op if the relevant experiment is not enabled.
- (void)renderAutofillTypePredictions:
    (const std::vector<autofill::FormStructure*>&)forms;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_AUTOFILL_AGENT_H_
