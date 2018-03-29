// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/autofill_agent.h"

#include <memory>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/js_autofill_manager.h"
#include "components/prefs/pref_service.h"
#include "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_state/form_activity_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using FormDataVector = std::vector<autofill::FormData>;

// The type of the completion handler block for
// |fetchFormsWithName:minimumRequiredFieldsCount:pageURL:completionHandler|
typedef void (^FetchFormsCompletionHandler)(BOOL, const FormDataVector&);

// Gets the first focusable form and field specified by |fieldIdentifier| from
// |forms|, modifying the returned field so that input elements are also
// handled.
void GetFormAndField(autofill::FormData* form,
                     autofill::FormFieldData* field,
                     const FormDataVector& forms,
                     const std::string& fieldIdentifier,
                     const std::string& type) {
  DCHECK_GE(forms.size(), 1U);
  *form = forms[0];
  const base::string16 fieldIdentifier16 = base::UTF8ToUTF16(fieldIdentifier);
  for (const auto& currentField : form->fields) {
    if (currentField.id == fieldIdentifier16 && currentField.is_focusable) {
      *field = currentField;
      break;
    }
  }
  if (field->SameFieldAs(autofill::FormFieldData()))
    return;

  // Hack to get suggestions from select input elements.
  if (field->form_control_type == "select-one") {
    // Any value set will cause the AutofillManager to filter suggestions (only
    // show suggestions that begin the same as the current value) with the
    // effect that one only suggestion would be returned; the value itself.
    field->value = base::string16();
  }
}

}  // namespace

@interface AutofillAgent ()<CRWWebStateObserver>

// Notifies the autofill manager when forms are detected on a page.
- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
                  ofFormsSeen:(const FormDataVector&)forms;

// Notifies the autofill manager when forms are submitted.
- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
             ofFormsSubmitted:(const FormDataVector&)forms
                userInitiated:(BOOL)userInitiated;

// Invokes the form extraction script and loads the output into the format
// expected by the AutofillManager.
// If |filtered| is NO, all forms are extracted.
// If |filtered| is YES,
//   - if |formName| is non-empty, only a form of that name is extracted.
//   - if |formName| is empty, unowned fields are extracted.
// Only forms with at least |requiredFieldsCount| fields are extracted.
// Calls |completionHandler| with a success BOOL of YES and the form data that
// was extracted.
// Calls |completionHandler| with NO if the forms could not be extracted.
// |completionHandler| cannot be nil.
- (void)fetchFormsFiltered:(BOOL)filtered
                      withName:(const base::string16&)formName
    minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                       pageURL:(const GURL&)pageURL
             completionHandler:(FetchFormsCompletionHandler)completionHandler;

// Processes the JSON form data extracted from the page when form activity is
// detected and informs the AutofillManager.
- (void)processFormActivityExtractedData:(const FormDataVector&)forms
                               fieldName:(const std::string&)fieldName
                         fieldIdentifier:(const std::string&)fieldIdentifier
                                    type:(const std::string&)type
                                webState:(web::WebState*)webState;

// Returns whether Autofill is enabled by checking if Autofill is turned on and
// if the current URL has a web scheme and the page content is HTML.
- (BOOL)isAutofillEnabled;

// Sends a request to AutofillManager to retrieve suggestions for the specified
// form and field.
- (void)queryAutofillWithForms:(const FormDataVector&)forms
                     fieldName:(NSString*)fieldName
               fieldIdentifier:(NSString*)fieldIdentifier
                          type:(NSString*)type
                    typedValue:(NSString*)typedValue
                      webState:(web::WebState*)webState
             completionHandler:(SuggestionsAvailableCompletion)completion;

// Rearranges and filters the suggestions to move profile or credit card
// suggestions to the front if the user has selected one recently and remove
// key/value suggestions if the user hasn't started typing.
- (NSArray*)processSuggestions:(NSArray*)suggestions;

// Sends the the |formData| to the JavaScript manager of |webState_| to actually
// fill the data.
- (void)sendDataToWebState:(NSString*)formData;

@end

@implementation AutofillAgent {
  // Whether page is scanned for forms.
  BOOL pageProcessed_;

  // The WebState this instance is observing. Will be null after
  // -webStateDestroyed: has been called.
  web::WebState* webState_;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // The pref service for which this agent was created.
  PrefService* prefService_;

  // Manager for Autofill JavaScripts.
  JsAutofillManager* jsAutofillManager_;

  // The name of the most recent autocomplete field; tracks the currently-
  // focused form element in order to force filling of the currently selected
  // form element, even if it's non-empty.
  base::string16 pendingAutocompleteField_;
  // The identifier of the most recent suggestion accepted by the user. Only
  // used to reorder future suggestion lists, placing matching suggestions first
  // in the list.
  NSInteger mostRecentSelectedIdentifier_;

  // Suggestions state:
  // The most recent form suggestions.
  NSArray* mostRecentSuggestions_;
  // The completion to inform FormSuggestionController that a user selection
  // has been handled.
  SuggestionHandledCompletion suggestionHandledCompletion_;
  // The completion to inform FormSuggestionController that suggestions are
  // available for a given form and field.
  SuggestionsAvailableCompletion suggestionsAvailableCompletion_;
  // The text entered by the user into the active field.
  NSString* typedValue_;
  // Popup delegate for the most recent suggestions.
  // The reference is weak because a weak pointer is sent to our
  // AutofillManagerDelegate.
  base::WeakPtr<autofill::AutofillPopupDelegate> popupDelegate_;
  // The autofill data that needs to be send when the |webState_| is shown.
  // The string is in JSON format.
  NSString* pendingFormJSON_;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState {
  DCHECK(prefService);
  DCHECK(webState);
  self = [super init];
  if (self) {
    webState_ = webState;
    prefService_ = prefService;
    webStateObserverBridge_ =
        std::make_unique<web::WebStateObserverBridge>(self);
    webState_->AddObserver(webStateObserverBridge_.get());
    jsAutofillManager_ = [[JsAutofillManager alloc]
        initWithReceiver:webState_->GetJSInjectionReceiver()];
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)dealloc {
  if (webState_) {
    webState_->RemoveObserver(webStateObserverBridge_.get());
    webStateObserverBridge_.reset();
    webState_ = nullptr;
  }
}

- (void)detachFromWebState {
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  if (webState_) {
    webState_->RemoveObserver(webStateObserverBridge_.get());
    webStateObserverBridge_.reset();
    webState_ = nullptr;
  }
}

#pragma mark -
#pragma mark Private

// Returns the autofill manager associated with a web::WebState instance.
// Returns nullptr if there is no autofill manager associated anymore, this can
// happen when |close| has been called on the |webState|. Also returns nullptr
// if detachFromWebState has been called.
- (autofill::AutofillManager*)autofillManagerFromWebState:
    (web::WebState*)webState {
  if (!webState || !webStateObserverBridge_)
    return nullptr;
  return autofill::AutofillDriverIOS::FromWebState(webState)
      ->autofill_manager();
}

- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
                  ofFormsSeen:(const FormDataVector&)forms {
  DCHECK(autofillManager);
  DCHECK(!forms.empty());
  autofillManager->Reset();
  autofillManager->OnFormsSeen(forms, base::TimeTicks::Now());
}

- (void)notifyAutofillManager:(autofill::AutofillManager*)autofillManager
             ofFormsSubmitted:(const FormDataVector&)forms
                userInitiated:(BOOL)userInitiated {
  DCHECK(autofillManager);
  // Exactly one form should be extracted.
  DCHECK_EQ(1U, forms.size());
  autofill::FormData form = forms[0];
  autofillManager->OnFormSubmitted(form, false,
                                   autofill::SubmissionSource::FORM_SUBMISSION,
                                   base::TimeTicks::Now());
  autofill::KeyboardAccessoryMetricsLogger::OnFormSubmitted();
}

- (void)fetchFormsFiltered:(BOOL)filtered
                      withName:(const base::string16&)formName
    minimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                       pageURL:(const GURL&)pageURL
             completionHandler:(FetchFormsCompletionHandler)completionHandler {
  DCHECK(completionHandler);

  // Necessary so the values can be used inside a block.
  base::string16 formNameCopy = formName;
  GURL pageURLCopy = pageURL;
  [jsAutofillManager_
      fetchFormsWithMinimumRequiredFieldsCount:requiredFieldsCount
                             completionHandler:^(NSString* formJSON) {
                               std::vector<autofill::FormData> formData;
                               bool success = autofill::ExtractFormsData(
                                   formJSON, filtered, formNameCopy,
                                   pageURLCopy, &formData);
                               completionHandler(success, formData);
                             }];
}

- (NSArray*)processSuggestions:(NSArray*)suggestions {
  // The suggestion array is cloned (to claim ownership) and to slightly
  // reorder; a future improvement is to base order on text typed in other
  // fields by users as well as accepted suggestions (crbug.com/245261).
  NSMutableArray* suggestionsCopy = [suggestions mutableCopy];

  // If the most recently selected suggestion was a profile or credit card
  // suggestion, move it to the front of the suggestions.
  if (mostRecentSelectedIdentifier_ > 0) {
    NSUInteger idx = [suggestionsCopy
        indexOfObjectPassingTest:^BOOL(id obj, NSUInteger, BOOL*) {
          FormSuggestion* suggestion = obj;
          return suggestion.identifier == mostRecentSelectedIdentifier_;
        }];

    if (idx != NSNotFound) {
      FormSuggestion* suggestion = suggestionsCopy[idx];
      [suggestionsCopy removeObjectAtIndex:idx];
      [suggestionsCopy insertObject:suggestion atIndex:0];
    }
  }

  // Filter out any key/value suggestions if the user hasn't typed yet.
  if ([typedValue_ length] == 0) {
    for (NSInteger idx = [suggestionsCopy count] - 1; idx >= 0; idx--) {
      FormSuggestion* suggestion = suggestionsCopy[idx];
      if (suggestion.identifier == autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
        [suggestionsCopy removeObjectAtIndex:idx];
      }
    }
  }

  // If "clear form" entry exists then move it to the front of the suggestions.
  for (NSInteger idx = [suggestionsCopy count] - 1; idx > 0; idx--) {
    FormSuggestion* suggestion = suggestionsCopy[idx];
    if (suggestion.identifier == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
      FormSuggestion* suggestionToMove = suggestionsCopy[idx];
      [suggestionsCopy removeObjectAtIndex:idx];
      [suggestionsCopy insertObject:suggestionToMove atIndex:0];
      break;
    }
  }

  return suggestionsCopy;
}

- (void)onSuggestionsReady:(NSArray*)suggestions
             popupDelegate:
                 (const base::WeakPtr<autofill::AutofillPopupDelegate>&)
                     delegate {
  popupDelegate_ = delegate;
  mostRecentSuggestions_ = [[self processSuggestions:suggestions] copy];
  if (suggestionsAvailableCompletion_)
    suggestionsAvailableCompletion_([mostRecentSuggestions_ count] > 0);
  suggestionsAvailableCompletion_ = nil;
}

#pragma mark -
#pragma mark FormSuggestionProvider

- (void)queryAutofillWithForms:(const FormDataVector&)forms
                     fieldName:(NSString*)fieldName
               fieldIdentifier:(NSString*)fieldIdentifier
                          type:(NSString*)type
                    typedValue:(NSString*)typedValue
                      webState:(web::WebState*)webState
             completionHandler:(SuggestionsAvailableCompletion)completion {
  autofill::AutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState];
  if (!autofillManager)
    return;

  // Passed to delegates; we don't use it so it's set to zero.
  int queryId = 0;

  // Find the right form and field.
  autofill::FormFieldData field;
  autofill::FormData form;
  GetFormAndField(&form, &field, forms,
                  base::SysNSStringToUTF8(fieldIdentifier),
                  base::SysNSStringToUTF8(type));

  // Save the completion and go look for suggestions.
  suggestionsAvailableCompletion_ = [completion copy];
  typedValue_ = [typedValue copy];

  // Query the AutofillManager for suggestions. Results will arrive in
  // [AutofillController showAutofillPopup].
  autofillManager->OnQueryFormFieldAutofill(queryId, form, field, gfx::RectF());
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                                 fieldName:(NSString*)fieldName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                               isMainFrame:(BOOL)isMainFrame
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  DCHECK_EQ(webState_, webState);

  if (!isMainFrame) {
    // Filling in iframes is not implemented.
    completion(NO);
    return;
  }

  if (![self isAutofillEnabled]) {
    completion(NO);
    return;
  }

  // Once the active form and field are extracted, send a query to the
  // AutofillManager for suggestions.
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    if (success && forms.size() == 1) {
      [weakSelf queryAutofillWithForms:forms
                             fieldName:fieldName
                       fieldIdentifier:fieldIdentifier
                                  type:type
                            typedValue:typedValue
                              webState:webState
                     completionHandler:completion];
    }
  };

  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));

  // Re-extract the active form and field only. All forms with at least one
  // input element are considered because key/value suggestions are offered
  // even on short forms.
  [self fetchFormsFiltered:YES
                        withName:base::SysNSStringToUTF16(formName)
      minimumRequiredFieldsCount:1
                         pageURL:pageURL
               completionHandler:completionHandler];
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                         fieldName:(NSString*)fieldName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  DCHECK(mostRecentSuggestions_)
      << "Requestor should have called "
      << "|checkIfSuggestionsAvailableForForm:fieldName:fieldIdentifier:type:"
      << "completionHandler:| "
      << "and waited for the result before calling "
      << "|retrieveSuggestionsForForm:field:type:completionHandler:|.";
  completion(mostRecentSuggestions_, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                  fieldName:(NSString*)fieldName
            fieldIdentifier:(NSString*)fieldIdentifier
                       form:(NSString*)formName
          completionHandler:(SuggestionHandledCompletion)completion {
  [[UIDevice currentDevice] playInputClick];
  suggestionHandledCompletion_ = [completion copy];
  mostRecentSelectedIdentifier_ = suggestion.identifier;

  if (suggestion.identifier > 0) {
    pendingAutocompleteField_ = base::SysNSStringToUTF16(fieldIdentifier);
    if (popupDelegate_) {
      popupDelegate_->DidAcceptSuggestion(
          base::SysNSStringToUTF16(suggestion.value), suggestion.identifier, 0);
    }
  } else if (suggestion.identifier ==
             autofill::POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // FormSuggestion is a simple, single value that can be filled out now.
    [self fillField:base::SysNSStringToUTF8(fieldIdentifier)
           formName:base::SysNSStringToUTF8(formName)
              value:base::SysNSStringToUTF16(suggestion.value)];
  } else if (suggestion.identifier == autofill::POPUP_ITEM_ID_CLEAR_FORM) {
    [jsAutofillManager_
        clearAutofilledFieldsForFormNamed:formName
                        completionHandler:suggestionHandledCompletion_];
    suggestionHandledCompletion_ = nil;
  } else {
    NOTREACHED() << "unknown identifier " << suggestion.identifier;
  }
}

#pragma mark -
#pragma mark CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  [self detachFromWebState];
}

- (void)webStateWasShown:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  if (pendingFormJSON_) {
    [self sendDataToWebState:pendingFormJSON_];
  }
  pendingFormJSON_ = nil;
}

- (void)webState:(web::WebState*)webState
    didSubmitDocumentWithFormNamed:(const std::string&)formName
                     userInitiated:(BOOL)userInitiated
                       isMainFrame:(BOOL)isMainFrame {
  if (!isMainFrame) {
    // Saving from iframes is not implemented.
    return;
  }

  if (![self isAutofillEnabled])
    return;

  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    AutofillAgent* strongSelf = weakSelf;
    if (!strongSelf || !success)
      return;
    autofill::AutofillManager* autofillManager =
        [strongSelf autofillManagerFromWebState:webState];
    if (!autofillManager || forms.empty())
      return;
    if (forms.size() > 1) {
      DLOG(WARNING) << "Only one form should be extracted.";
      return;
    }
    [strongSelf notifyAutofillManager:autofillManager
                     ofFormsSubmitted:forms
                        userInitiated:userInitiated];

  };

  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));

  // This code is racing against the new page loading and will not get the
  // password form data if the page has changed. In most cases this code wins
  // the race.
  // TODO(crbug.com/418827): Fix this by passing in more data from the JS side.
  [self fetchFormsFiltered:YES
                        withName:base::UTF8ToUTF16(formName)
      minimumRequiredFieldsCount:1
                         pageURL:pageURL
               completionHandler:completionHandler];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  if (![self isAutofillEnabled])
    return;

  pageProcessed_ = NO;
  [self processPage:webState];
}

- (void)processPage:(web::WebState*)webState {
  // This process is only done once.
  if (pageProcessed_)
    return;
  pageProcessed_ = YES;
  [jsAutofillManager_ addJSDelay];

  popupDelegate_.reset();
  suggestionsAvailableCompletion_ = nil;
  suggestionHandledCompletion_ = nil;
  mostRecentSuggestions_ = nil;
  typedValue_ = nil;

  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  [jsAutofillManager_ trackFormUpdates];
  [self scanFormsInPage:webState pageURL:pageURL];
}

- (void)scanFormsInPage:(web::WebState*)webState pageURL:(const GURL&)pageURL {
  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    AutofillAgent* strongSelf = weakSelf;
    if (!strongSelf || !success)
      return;
    autofill::AutofillManager* autofillManager =
        [strongSelf autofillManagerFromWebState:webState];
    if (!autofillManager || forms.empty())
      return;
    [strongSelf notifyAutofillManager:autofillManager ofFormsSeen:forms];
  };
  // The document has now been fully loaded. Scan for forms to be extracted.
  size_t min_required_fields =
      MIN(autofill::MinRequiredFieldsForUpload(),
          MIN(autofill::MinRequiredFieldsForHeuristics(),
              autofill::MinRequiredFieldsForQuery()));
  [self fetchFormsFiltered:NO
                        withName:base::string16()
      minimumRequiredFieldsCount:min_required_fields
                         pageURL:pageURL
               completionHandler:completionHandler];
}

- (void)webState:(web::WebState*)webState
    didRegisterFormActivity:(const web::FormActivityParams&)params {
  if (![self isAutofillEnabled])
    return;

  // Returns early and reset the suggestion state if an error occurs.
  if (params.input_missing)
    return;

  // Processing the page can be needed here if Autofill is enabled in settings
  // when the page is already loaded, or if the user focuses a field before the
  // page is fully loaded.
  [self processPage:webState];

  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));

  // If the event is a form_changed, then the event concerns the whole page and
  // not a particular form. The whole page need to be reparsed to find the new
  // forms.
  if (params.type.compare("form_changed") == 0) {
    [self scanFormsInPage:webState pageURL:pageURL];
    return;
  }

  // Blur not handled; we don't reset the suggestion state because if the
  // keyboard is about to be dismissed there's no point. If not it means the
  // next focus event will update the suggestion state within milliseconds, so
  // if we do it now a flicker will be seen.
  if (params.type.compare("blur") == 0)
    return;

  // Necessary so the strings can be used inside a block.
  std::string fieldNameCopy = params.field_name;
  std::string fieldIdentifierCopy = params.field_identifier;
  std::string typeCopy = params.type;

  __weak AutofillAgent* weakSelf = self;
  id completionHandler = ^(BOOL success, const FormDataVector& forms) {
    if (success && forms.size() == 1) {
      [weakSelf processFormActivityExtractedData:forms
                                       fieldName:fieldNameCopy
                                 fieldIdentifier:fieldIdentifierCopy
                                            type:typeCopy
                                        webState:webState];
    }
  };

  // Re-extract the active form and field only. There is no minimum field
  // requirement because key/value suggestions are offered even on short forms.
  [self fetchFormsFiltered:YES
                        withName:base::UTF8ToUTF16(params.form_name)
      minimumRequiredFieldsCount:1
                         pageURL:pageURL
               completionHandler:completionHandler];
}

#pragma mark - Private methods.

- (void)processFormActivityExtractedData:(const FormDataVector&)forms
                               fieldName:(const std::string&)fieldName
                         fieldIdentifier:(const std::string&)fieldIdentifier
                                    type:(const std::string&)type
                                webState:(web::WebState*)webState {
  DCHECK_EQ(webState_, webState);
  autofill::AutofillManager* autofillManager =
      [self autofillManagerFromWebState:webState];
  if (!autofillManager)
    return;

  autofill::FormFieldData field;
  autofill::FormData form;
  GetFormAndField(&form, &field, forms, fieldIdentifier, type);

  // Tell the manager about the form activity (for metrics).
  if (type.compare("input") == 0 && (field.form_control_type == "text" ||
                                     field.form_control_type == "password")) {
    autofillManager->OnTextFieldDidChange(form, field, gfx::RectF(),
                                          base::TimeTicks::Now());
  }
}

- (BOOL)isAutofillEnabled {
  if (!prefService_->GetBoolean(autofill::prefs::kAutofillEnabled))
    return NO;

  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState_->GetCurrentURL(&trustLevel));

  // Only web URLs are supported by Autofill.
  return web::UrlHasWebScheme(pageURL) && webState_->ContentIsHTML();
}

#pragma mark -
#pragma mark AutofillViewClient

// Complete a field named |fieldName| on the form named |formName| using |value|
// then move the cursor.
// TODO(crbug.com/661621): |dataString| ends up at fillFormField() in
// autofill_controller.js. fillFormField() expects an AutofillFormFieldData
// object, which |dataString| is not because 'form' is not a specified member of
// AutofillFormFieldData. fillFormField() also expects members 'max_length' and
// 'is_checked' to exist.
- (void)fillField:(const std::string&)fieldIdentifier
         formName:(const std::string&)formName
            value:(const base::string16)value {
  base::DictionaryValue data;
  data.SetString("identifier", fieldIdentifier);
  data.SetString("form", formName);
  data.SetString("value", value);
  std::string dataString;
  base::JSONWriter::Write(data, &dataString);

  DCHECK(suggestionHandledCompletion_);
  [jsAutofillManager_ fillActiveFormField:base::SysUTF8ToNSString(dataString)
                        completionHandler:suggestionHandledCompletion_];
  suggestionHandledCompletion_ = nil;
}

- (void)onFormDataFilled:(const autofill::FormData&)form {
  std::unique_ptr<base::DictionaryValue> JSONForm(new base::DictionaryValue);
  JSONForm->SetString("formName", base::UTF16ToUTF8(form.name));
  // Note: Destruction of all child base::Value types is handled by the root
  // formData object on its own destruction.
  auto JSONFields = std::make_unique<base::DictionaryValue>();

  const std::vector<autofill::FormFieldData>& autofillFields = form.fields;
  for (const auto& autofillField : autofillFields) {
    if (JSONFields->HasKey(base::UTF16ToUTF8(autofillField.id)) &&
        autofillField.value.empty())
      continue;
    JSONFields->SetKey(base::UTF16ToUTF8(autofillField.id),
                       base::Value(autofillField.value));
  }
  JSONForm->Set("fields", std::move(JSONFields));

  // Stringify the JSON data and send it to the UIWebView-side fillForm method.
  std::string JSONString;
  base::JSONWriter::Write(*JSONForm.get(), &JSONString);
  NSString* nsJSONString = base::SysUTF8ToNSString(JSONString);

  if (!webState_->IsVisible()) {
    pendingFormJSON_ = nsJSONString;
    return;
  }
  [self sendDataToWebState:nsJSONString];
}

- (void)sendDataToWebState:(NSString*)JSONData {
  DCHECK(webState_->IsVisible());
  // It is possible that the fill was not initiated by selecting a suggestion.
  // In this case we provide an empty callback.
  if (!suggestionHandledCompletion_)
    suggestionHandledCompletion_ = [^{
    } copy];
  [jsAutofillManager_ fillForm:JSONData
      forceFillFieldIdentifier:base::SysUTF16ToNSString(
                                   pendingAutocompleteField_)
             completionHandler:suggestionHandledCompletion_];
  suggestionHandledCompletion_ = nil;
}

// Returns the first value from the array of possible values that has a match in
// the list of accepted values in the AutofillField, or nil if there is no
// match. If AutofillField does not specify valid values, the first value is
// returned from the list.
+ (NSString*)selectFrom:(NSArray*)values for:(autofill::AutofillField*)field {
  if (field->option_values.empty())
    return values[0];

  for (NSString* value in values) {
    std::string valueString = base::SysNSStringToUTF8(value);
    for (size_t i = 0; i < field->option_values.size(); ++i) {
      if (base::UTF16ToUTF8(field->option_values[i]) == valueString)
        return value;
    }
    for (size_t i = 0; i < field->option_contents.size(); ++i) {
      if (base::UTF16ToUTF8(field->option_contents[i]) == valueString)
        return value;
    }
  }

  return nil;
}

- (void)renderAutofillTypePredictions:
    (const std::vector<autofill::FormDataPredictions>&)forms {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillShowTypePredictions)) {
    return;
  }

  base::DictionaryValue predictionData;
  for (const auto& form : forms) {
    auto formJSONData = std::make_unique<base::DictionaryValue>();
    DCHECK(form.fields.size() == form.data.fields.size());
    for (size_t i = 0; i < form.fields.size(); i++) {
      formJSONData->SetKey(base::UTF16ToUTF8(form.data.fields[i].id),
                           base::Value(form.fields[i].overall_type));
    }
    predictionData.SetWithoutPathExpansion(base::UTF16ToUTF8(form.data.name),
                                           std::move(formJSONData));
  }
  std::string dataString;
  base::JSONWriter::Write(predictionData, &dataString);
  [jsAutofillManager_ fillPredictionData:base::SysUTF8ToNSString(dataString)];
}

@end
