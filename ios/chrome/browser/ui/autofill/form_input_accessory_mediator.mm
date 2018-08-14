// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"

#include "base/ios/block_types.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "ios/chrome/browser/autofill/form_input_accessory_consumer.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/url_scheme_util.h"
#include "ios/web/public/web_state/form_activity_params.h"
#import "ios/web/public/web_state/js/crw_js_injection_receiver.h"
#include "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryMediator ()<FormActivityObserver,
                                         CRWWebStateObserver,
                                         WebStateListObserving>
@property(nonatomic, strong)
    FormInputAccessoryViewHandler* formInputAccessoryHandler;

// The WebState this instance is observing. Can be null.
@property(nonatomic, assign) web::WebState* webState;

// The JS manager for interacting with the underlying form.
@property(nonatomic, weak) JsSuggestionManager* JSSuggestionManager;

// The main consumer for this mediator.
@property(nonatomic, weak) id<FormInputAccessoryConsumer> consumer;

// The object that manages the currently-shown custom accessory view.
@property(nonatomic, weak) id<FormInputAccessoryViewProvider> currentProvider;

// The objects that can provide a custom input accessory view while filling
// forms.
@property(nonatomic, copy)
    NSArray<id<FormInputAccessoryViewProvider>>* providers;

@end

@implementation FormInputAccessoryMediator {
  // The WebStateList this instance is observing in order to update the
  // active WebState.
  WebStateList* _webStateList;

  // Bridge to observe the web state list from Objective-C.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Bridge to observe form activity in |_webState|.
  std::unique_ptr<autofill::FormActivityObserverBridge>
      _formActivityObserverBridge;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;
}

@synthesize consumer = _consumer;
@synthesize currentProvider = _currentProvider;
@synthesize formInputAccessoryHandler = _formInputAccessoryHandler;
@synthesize JSSuggestionManager = _JSSuggestionManager;
@synthesize providers = _providers;
@synthesize webState = _webState;

- (instancetype)initWithConsumer:(id<FormInputAccessoryConsumer>)consumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _consumer = consumer;
    if (webStateList) {
      _webStateList = webStateList;
      _webStateListObserver =
          std::make_unique<WebStateListObserverBridge>(self);
      _webStateList->AddObserver(_webStateListObserver.get());
      web::WebState* webState = webStateList->GetActiveWebState();
      if (webState) {
        _webState = webState;
        CRWJSInjectionReceiver* injectionReceiver =
            webState->GetJSInjectionReceiver();
        _JSSuggestionManager = base::mac::ObjCCastStrict<JsSuggestionManager>(
            [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
        _providers = @[ FormSuggestionTabHelper::FromWebState(webState)
                            ->GetAccessoryViewProvider() ];
        _formActivityObserverBridge =
            std::make_unique<autofill::FormActivityObserverBridge>(_webState,
                                                                   self);
        _webStateObserverBridge =
            std::make_unique<web::WebStateObserverBridge>(self);
        webState->AddObserver(_webStateObserverBridge.get());
      }
    }
    _formInputAccessoryHandler = [[FormInputAccessoryViewHandler alloc] init];
    _formInputAccessoryHandler.JSSuggestionManager = _JSSuggestionManager;

    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(handleTextInputDidBeginEditing:)
                          name:UITextFieldTextDidBeginEditingNotification
                        object:nil];
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
    _formActivityObserverBridge.reset();
  }
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
  _formActivityObserverBridge.reset();
}

- (void)detachFromWebState {
  [self reset];
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
    _webStateObserverBridge.reset();
    _webState = nullptr;
    _formActivityObserverBridge.reset();
  }
}

#pragma mark - FormActivityObserver

- (void)webState:(web::WebState*)webState
    registeredFormActivity:(const web::FormActivityParams&)params {
  DCHECK_EQ(_webState, webState);
  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  if (params.input_missing ||
      trustLevel != web::URLVerificationTrustLevel::kAbsolute ||
      !web::UrlHasWebScheme(pageURL) || !webState->ContentIsHTML()) {
    [self reset];
    return;
  }

  if (params.type == "blur" || params.type == "change" ||
      params.type == "form_changed") {
    return;
  }

  [self retrieveAccessoryViewForForm:params webState:webState];
}

#pragma mark - CRWWebStateObserver

- (void)webStateWasHidden:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self reset];
}

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self reset];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self detachFromWebState];
}

#pragma mark - CRWWebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  [self updateWithNewWebState:newWebState];
}

#pragma mark - Private

// Updates the accessory mediator with the passed web state, its JS suggestion
// manager and the registered providers. If NULL is passed it will instead clear
// those properties in the mediator.
- (void)updateWithNewWebState:(web::WebState*)webState {
  [self detachFromWebState];
  if (webState) {
    self.webState = webState;
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    webState->AddObserver(_webStateObserverBridge.get());
    _formActivityObserverBridge =
        std::make_unique<autofill::FormActivityObserverBridge>(webState, self);
    CRWJSInjectionReceiver* injectionReceiver =
        webState->GetJSInjectionReceiver();
    self.JSSuggestionManager = base::mac::ObjCCastStrict<JsSuggestionManager>(
        [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
    self.providers = @[ FormSuggestionTabHelper::FromWebState(webState)
                            ->GetAccessoryViewProvider() ];
    _formInputAccessoryHandler.JSSuggestionManager = self.JSSuggestionManager;
  } else {
    self.webState = NULL;
    self.JSSuggestionManager = nil;
    self.providers = @[];
  }
}

// Resets the current provider, the handler and the consumer to a clean state.
- (void)reset {
  [self.consumer restoreDefaultInputAccessoryView];

  [self.currentProvider inputAccessoryViewControllerDidReset];
  self.currentProvider = nil;

  [self.formInputAccessoryHandler reset];
}

// Asynchronously queries the providers for an accessory view. Sends it to
// the consumer if found.
- (void)retrieveAccessoryViewForForm:(const web::FormActivityParams&)params
                            webState:(web::WebState*)webState {
  DCHECK_EQ(webState, self.webState);
  // Build a block for each provider that will invoke its completion with YES
  // if the provider can provide an accessory view for the specified form/field
  // and NO otherwise.
  NSMutableArray* findProviderBlocks = [[NSMutableArray alloc] init];
  for (id<FormInputAccessoryViewProvider> provider in _providers) {
    passwords::PipelineBlock findProviderBlock =
        [self queryViewBlockForProvider:provider params:params];
    [findProviderBlocks addObject:findProviderBlock];
  }

  // Run all the blocks in |findProviderBlocks| until one invokes its
  // completion with YES. The first one to do so will be passed to
  // |onProviderFound|.
  __weak __typeof(self) weakSelf = self;
  passwords::RunSearchPipeline(findProviderBlocks, ^(NSUInteger providerIndex) {
    // If no view was retrieved, reset self.
    if (providerIndex == NSNotFound) {
      [weakSelf reset];
    }
  });
}

// Returns a pipeline block used to search for a provider with the current form
// params.
- (passwords::PipelineBlock)
queryViewBlockForProvider:(id<FormInputAccessoryViewProvider>)provider
                   params:(web::FormActivityParams)params {
  __weak __typeof(self) weakSelf = self;
  return ^(void (^completion)(BOOL success)) {
    FormInputAccessoryMediator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    AccessoryViewReadyCompletion accessoryViewReadyCompletion =
        [self accessoryViewReadyBlockWithCompletion:completion];
    [provider retrieveAccessoryViewForForm:params
                                  webState:strongSelf.webState
                  accessoryViewUpdateBlock:accessoryViewReadyCompletion];
  };
}

// Returns a block setting up the provider and the view returned. It calls the
// passed completion with NO if the view found is invalid. With YES otherwise.
- (AccessoryViewReadyCompletion)accessoryViewReadyBlockWithCompletion:
    (void (^)(BOOL success))completion {
  __weak __typeof(self) weakSelf = self;
  return ^(UIView* accessoryView, id<FormInputAccessoryViewProvider> provider) {
    // View is nil, tell the pipeline to continue searching.
    if (!accessoryView) {
      completion(NO);
      return;
    }
    // Once the view is retrieved, tell the pipeline to stop and
    // update the UI.
    completion(YES);
    FormInputAccessoryMediator* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    if (strongSelf.currentProvider != provider) {
      [strongSelf.currentProvider inputAccessoryViewControllerDidReset];
    }
    strongSelf.currentProvider = provider;
    [provider setAccessoryViewDelegate:strongSelf.formInputAccessoryHandler];
    [strongSelf.consumer
        showCustomInputAccessoryView:accessoryView
                  navigationDelegate:strongSelf.formInputAccessoryHandler];
  };
}
// When any text field or text view (e.g. omnibox, settings, card unmask dialog)
// begins editing, reset ourselves so that we don't present our custom view over
// the keyboard.
- (void)handleTextInputDidBeginEditing:(NSNotification*)notification {
  [self reset];
}

#pragma mark - Tests

- (void)injectWebState:(web::WebState*)webState {
  [self detachFromWebState];
  _webState = webState;
  if (!_webState) {
    return;
  }
  _webStateObserverBridge = std::make_unique<web::WebStateObserverBridge>(self);
  _webState->AddObserver(_webStateObserverBridge.get());
  _formActivityObserverBridge =
      std::make_unique<autofill::FormActivityObserverBridge>(_webState, self);
}

- (void)injectSuggestionManager:(JsSuggestionManager*)JSSuggestionManager {
  _JSSuggestionManager = JSSuggestionManager;
  _formInputAccessoryHandler.JSSuggestionManager = _JSSuggestionManager;
}

- (void)injectProviders:
    (NSArray<id<FormInputAccessoryViewProvider>>*)providers {
  self.providers = providers;
}

@end
