// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_suggestion_controller.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/ios/weak_nsobject.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/form_suggestion_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/crw_web_view_proxy.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#import "ios/web/public/web_state/web_state.h"

namespace {

// Struct that describes suggestion state.
struct AutofillSuggestionState {
  AutofillSuggestionState(const std::string& form_name,
                          const std::string& field_name,
                          const std::string& typed_value);
  // The name of the form for autofill.
  std::string form_name;
  // The name of the field for autofill.
  std::string field_name;
  // The user-typed value in the field.
  std::string typed_value;
  // The suggestions for the form field. An array of |FormSuggestion|.
  base::scoped_nsobject<NSArray> suggestions;
};

AutofillSuggestionState::AutofillSuggestionState(const std::string& form_name,
                                                 const std::string& field_name,
                                                 const std::string& typed_value)
    : form_name(form_name), field_name(field_name), typed_value(typed_value) {
}

}  // namespace

@interface FormSuggestionController () <FormInputAccessoryViewProvider> {
  // Form navigation delegate.
  base::WeakNSProtocol<id<FormInputAccessoryViewDelegate>> _delegate;

  // Callback to update the accessory view.
  base::mac::ScopedBlock<AccessoryViewReadyCompletion>
      accessoryViewUpdateBlock_;

  // Autofill suggestion state.
  std::unique_ptr<AutofillSuggestionState> _suggestionState;

  // Providers for suggestions, sorted according to the order in which
  // they should be asked for suggestions, with highest priority in front.
  base::scoped_nsobject<NSArray> _suggestionProviders;

  // Access to WebView from the CRWWebController.
  base::scoped_nsprotocol<id<CRWWebViewProxy>> _webViewProxy;
}

// Returns an autoreleased input accessory view that shows |suggestions|.
- (UIView*)suggestionViewWithSuggestions:(NSArray*)suggestions;

// Updates keyboard for |suggestionState|.
- (void)updateKeyboard:(AutofillSuggestionState*)suggestionState;

// Updates keyboard with |suggestions|.
- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions;

// Clears state in between page loads.
- (void)resetSuggestionState;

@end

@implementation FormSuggestionController {
  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Manager for FormSuggestion JavaScripts.
  base::scoped_nsobject<JsSuggestionManager> _jsSuggestionManager;

  // The provider for the current set of suggestions.
  __unsafe_unretained id<FormSuggestionProvider> _provider;  // weak
}

- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers
             JsSuggestionManager:(JsSuggestionManager*)jsSuggestionManager {
  self = [super init];
  if (self) {
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    _webViewProxy.reset([webState->GetWebViewProxy() retain]);
    _jsSuggestionManager.reset([jsSuggestionManager retain]);
    _suggestionProviders.reset([providers copy]);
  }
  return self;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers {
  JsSuggestionManager* jsSuggestionManager =
      base::mac::ObjCCast<JsSuggestionManager>(
          [webState->GetJSInjectionReceiver()
              instanceOfClass:[JsSuggestionManager class]]);
  return [self initWithWebState:webState
                      providers:providers
            JsSuggestionManager:jsSuggestionManager];
}

- (void)onNoSuggestionsAvailable {
}

- (void)detachFromWebState {
  _webStateObserverBridge.reset();
}

#pragma mark -
#pragma mark CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self detachFromWebState];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  [self processPage:webState];
}

- (void)processPage:(web::WebState*)webState {
  [self resetSuggestionState];

  web::URLVerificationTrustLevel trustLevel =
      web::URLVerificationTrustLevel::kNone;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  if (trustLevel != web::URLVerificationTrustLevel::kAbsolute) {
    DLOG(WARNING) << "Page load not handled on untrusted page";
    return;
  }

  if (web::UrlHasWebScheme(pageURL) && webState->ContentIsHTML())
    [_jsSuggestionManager inject];
}

- (void)setWebViewProxy:(id<CRWWebViewProxy>)webViewProxy {
  _webViewProxy.reset([webViewProxy retain]);
}

- (void)retrieveSuggestionsForFormNamed:(const std::string&)formName
                              fieldName:(const std::string&)fieldName
                                   type:(const std::string&)type
                               webState:(web::WebState*)webState {
  base::WeakNSObject<FormSuggestionController> weakSelf(self);
  base::scoped_nsobject<NSString> strongFormName(
      [base::SysUTF8ToNSString(formName) copy]);
  base::scoped_nsobject<NSString> strongFieldName(
      [base::SysUTF8ToNSString(fieldName) copy]);
  base::scoped_nsobject<NSString> strongType(
      [base::SysUTF8ToNSString(type) copy]);
  base::scoped_nsobject<NSString> strongValue(
      [base::SysUTF8ToNSString(_suggestionState.get()->typed_value) copy]);

  // Build a block for each provider that will invoke its completion with YES
  // if the provider can provide suggestions for the specified form/field/type
  // and NO otherwise.
  base::scoped_nsobject<NSMutableArray> findProviderBlocks(
      [[NSMutableArray alloc] init]);
  for (NSUInteger i = 0; i < [_suggestionProviders count]; i++) {
    base::mac::ScopedBlock<passwords::PipelineBlock> block(
        ^(void (^completion)(BOOL success)) {
          // Access all the providers through |self| to guarantee that both
          // |self| and all the providers exist when the block is executed.
          // |_suggestionProviders| is immutable, so the subscripting is
          // always valid.
          base::scoped_nsobject<FormSuggestionController> strongSelf(
              [weakSelf retain]);
          if (!strongSelf)
            return;
          id<FormSuggestionProvider> provider =
              strongSelf.get()->_suggestionProviders[i];
          [provider checkIfSuggestionsAvailableForForm:strongFormName
                                                 field:strongFieldName
                                                  type:strongType
                                            typedValue:strongValue
                                              webState:webState
                                     completionHandler:completion];
        },
        base::scoped_policy::RETAIN);
    [findProviderBlocks addObject:block];
  }

  // Once the suggestions are retrieved, update the suggestions UI.
  SuggestionsReadyCompletion readyCompletion =
      ^(NSArray* suggestions, id<FormSuggestionProvider> provider) {
        [weakSelf onSuggestionsReady:suggestions provider:provider];
      };

  // Once a provider is found, use it to retrieve suggestions.
  passwords::PipelineCompletionBlock completion = ^(NSUInteger providerIndex) {
    if (providerIndex == NSNotFound) {
      [weakSelf onNoSuggestionsAvailable];
      return;
    }
    base::scoped_nsobject<FormSuggestionController> strongSelf(
        [weakSelf retain]);
    if (!strongSelf)
      return;
    id<FormSuggestionProvider> provider =
        strongSelf.get()->_suggestionProviders[providerIndex];
    [provider retrieveSuggestionsForForm:strongFormName
                                   field:strongFieldName
                                    type:strongType
                              typedValue:strongValue
                                webState:webState
                       completionHandler:readyCompletion];
  };

  // Run all the blocks in |findProviderBlocks| until one invokes its
  // completion with YES. The first one to do so will be passed to
  // |completion|.
  passwords::RunSearchPipeline(findProviderBlocks, completion);
}

- (void)onSuggestionsReady:(NSArray*)suggestions
                  provider:(id<FormSuggestionProvider>)provider {
  // TODO(ios): crbug.com/249916. If we can also pass in the form/field for
  // which |suggestions| are, we should check here if |suggestions| are for
  // the current active element. If not, reset |_suggestionState|.
  if (!_suggestionState) {
    // The suggestion state was reset in between the call to Autofill API (e.g.
    // OnQueryFormFieldAutofill) and this method being called back. Results are
    // therefore no longer relevant.
    return;
  }

  _provider = provider;
  _suggestionState->suggestions.reset([suggestions copy]);
  [self updateKeyboard:_suggestionState.get()];
}

- (void)resetSuggestionState {
  _provider = nil;
  _suggestionState.reset();
}

- (void)clearSuggestions {
  // Note that other parts of the suggestionsState are not reset.
  if (!_suggestionState.get())
    return;
  _suggestionState->suggestions.reset([[NSArray alloc] init]);
  [self updateKeyboard:_suggestionState.get()];
}

- (void)updateKeyboard:(AutofillSuggestionState*)suggestionState {
  if (!suggestionState) {
    if (accessoryViewUpdateBlock_)
      accessoryViewUpdateBlock_.get()(nil, self);
  } else {
    [self updateKeyboardWithSuggestions:suggestionState->suggestions];
  }
}

- (void)updateKeyboardWithSuggestions:(NSArray*)suggestions {
  if (accessoryViewUpdateBlock_) {
    accessoryViewUpdateBlock_.get()(
        [self suggestionViewWithSuggestions:suggestions], self);
  }
}

- (UIView*)suggestionViewWithSuggestions:(NSArray*)suggestions {
  CGRect frame = [_webViewProxy keyboardAccessory].frame;
  // Force the desired height on iPad where the height of the
  // inputAccessoryView is 0.
  if (IsIPadIdiom()) {
    frame.size.height = autofill::kInputAccessoryHeight;
  }
  base::scoped_nsobject<FormSuggestionView> view([[FormSuggestionView alloc]
      initWithFrame:frame
             client:self
        suggestions:suggestions]);
  return view.autorelease();
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion {
  if (!_suggestionState)
    return;

  // Send the suggestion to the provider. Upon completion advance the cursor
  // for single-field Autofill, or close the keyboard for full-form Autofill.
  base::WeakNSObject<FormSuggestionController> weakSelf(self);
  [_provider
      didSelectSuggestion:suggestion
                 forField:base::SysUTF8ToNSString(_suggestionState->field_name)
                     form:base::SysUTF8ToNSString(_suggestionState->form_name)
        completionHandler:^{
          [[weakSelf accessoryViewDelegate] closeKeyboardWithoutButtonPress];
        }];
  _provider = nil;
}

- (id<FormInputAccessoryViewProvider>)accessoryViewProvider {
  return self;
}

#pragma mark FormInputAccessoryViewProvider

- (id<FormInputAccessoryViewDelegate>)accessoryViewDelegate {
  return _delegate.get();
}

- (void)setAccessoryViewDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  _delegate.reset(delegate);
}

- (void)
    checkIfAccessoryViewIsAvailableForFormNamed:(const std::string&)formName
                                      fieldName:(const std::string&)fieldName
                                       webState:(web::WebState*)webState
                              completionHandler:
                                  (AccessoryViewAvailableCompletion)
                                      completionHandler {
  [self processPage:webState];
  completionHandler(YES);
}

- (void)retrieveAccessoryViewForFormNamed:(const std::string&)formName
                                fieldName:(const std::string&)fieldName
                                    value:(const std::string&)value
                                     type:(const std::string&)type
                                 webState:(web::WebState*)webState
                 accessoryViewUpdateBlock:
                     (AccessoryViewReadyCompletion)accessoryViewUpdateBlock {
  _suggestionState.reset(
      new AutofillSuggestionState(formName, fieldName, value));
  accessoryViewUpdateBlock([self suggestionViewWithSuggestions:@[]], self);
  accessoryViewUpdateBlock_.reset([accessoryViewUpdateBlock copy]);
  [self retrieveSuggestionsForFormNamed:formName
                              fieldName:fieldName
                                   type:type
                               webState:webState];
}

- (void)inputAccessoryViewControllerDidReset:
        (FormInputAccessoryViewController*)controller {
  accessoryViewUpdateBlock_.reset();
  [self resetSuggestionState];
}

- (void)resizeAccessoryView {
  [self updateKeyboard:_suggestionState.get()];
}

- (BOOL)getLogKeyboardAccessoryMetrics {
  return YES;
}

@end
