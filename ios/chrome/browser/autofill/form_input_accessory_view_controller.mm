// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"

#include "base/ios/block_types.h"
#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "components/autofill/core/browser/keyboard_accessory_metrics_logger.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view.h"
#import "ios/chrome/browser/autofill/form_suggestion_view.h"
#import "ios/chrome/browser/passwords/password_generation_utils.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/web/public/test/crw_test_js_injection_receiver.h"
#import "ios/web/public/url_scheme_util.h"
#import "ios/web/public/web_state/crw_web_view_proxy.h"
#include "ios/web/public/web_state/url_verification_constants.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

namespace autofill {
NSString* const kFormSuggestionAssistButtonPreviousElement = @"previousTap";
NSString* const kFormSuggestionAssistButtonNextElement = @"nextTap";
NSString* const kFormSuggestionAssistButtonDone = @"done";
CGFloat const kInputAccessoryHeight = 44.0f;
}  // namespace autofill

namespace {

// Finds all views of a particular kind if class |klass| in the subview
// hierarchy of the given |root| view.
NSArray* FindDescendantsOfClass(UIView* root, Class klass) {
  DCHECK(root);
  NSMutableArray* viewsToExamine = [NSMutableArray arrayWithObject:root];
  NSMutableArray* descendants = [NSMutableArray array];

  while ([viewsToExamine count]) {
    UIView* view = [viewsToExamine lastObject];
    if ([view isKindOfClass:klass])
      [descendants addObject:view];

    [viewsToExamine removeLastObject];
    [viewsToExamine addObjectsFromArray:[view subviews]];
  }

  return descendants;
}

// Returns true if |item|'s action name contains |actionName|.
bool ItemActionMatchesName(UIBarButtonItem* item, NSString* actionName) {
  SEL itemAction = [item action];
  if (!itemAction)
    return false;
  NSString* itemActionName = NSStringFromSelector(itemAction);

  // We don't do a strict string match for the action name.
  return [itemActionName rangeOfString:actionName].location != NSNotFound;
}

// Finds all UIToolbarItems associated with a given UIToolbar |toolbar| with
// action selectors with a name that containts the action name specified by
// |actionName|.
NSArray* FindToolbarItemsForActionName(UIToolbar* toolbar,
                                       NSString* actionName) {
  NSMutableArray* toolbarItems = [NSMutableArray array];

  for (UIBarButtonItem* item in [toolbar items]) {
    if (ItemActionMatchesName(item, actionName))
      [toolbarItems addObject:item];
  }

  return toolbarItems;
}

// Finds all UIToolbarItem(s) with action selectors of the name specified by
// |actionName| in any UIToolbars in the view hierarchy below |root|.
NSArray* FindDescendantToolbarItemsForActionName(UIView* root,
                                                 NSString* actionName) {
  NSMutableArray* descendants = [NSMutableArray array];

  NSArray* toolbars = FindDescendantsOfClass(root, [UIToolbar class]);
  for (UIToolbar* toolbar in toolbars) {
    [descendants
        addObjectsFromArray:FindToolbarItemsForActionName(toolbar, actionName)];
  }

  return descendants;
}

#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
NSArray* FindDescendantToolbarItemsForActionName(
    UITextInputAssistantItem* inputAssistantItem,
    NSString* actionName) {
  NSMutableArray* toolbarItems = [NSMutableArray array];

  base::scoped_nsobject<NSMutableArray> buttonGroupsGroup(
      [[NSMutableArray alloc] init]);
  if (inputAssistantItem.leadingBarButtonGroups)
    [buttonGroupsGroup addObject:inputAssistantItem.leadingBarButtonGroups];
  if (inputAssistantItem.trailingBarButtonGroups)
    [buttonGroupsGroup addObject:inputAssistantItem.trailingBarButtonGroups];
  for (NSArray* buttonGroups in buttonGroupsGroup.get()) {
    for (UIBarButtonItemGroup* group in buttonGroups) {
      NSArray* items = group.barButtonItems;
      for (UIBarButtonItem* item in items) {
        if (ItemActionMatchesName(item, actionName))
          [toolbarItems addObject:item];
      }
    }
  }

  return toolbarItems;
}
#endif

// Computes the frame of each part of the accessory view of the keyboard. It is
// assumed that the keyboard has either two parts (when it is split) or one part
// (when it is merged).
//
// If there are two parts, the frame of the left part is returned in
// |leftFrame| and the frame of the right part is returned in |rightFrame|.
// If there is only one part, the frame is returned in |leftFrame| and
// |rightFrame| has size zero.
//
// Heuristics are used to compute this information. It returns false if the
// number of |inputAccessoryView.subviews| is not 2.
bool ComputeFramesOfKeyboardParts(UIView* inputAccessoryView,
                                  CGRect* leftFrame,
                                  CGRect* rightFrame) {
  // It is observed (on iOS 6) there are always two subviews in the original
  // input accessory view. When the keyboard is split, each subview represents
  // one part of the accesssary view of the keyboard. When the keyboard is
  // merged, one subview has the same frame as that of the whole accessory view
  // and the other has zero size with the screen width as origin.x.
  // The computation here is based on this observation.
  NSArray* subviews = inputAccessoryView.subviews;
  if (subviews.count != 2)
    return false;

  CGRect first_frame = static_cast<UIView*>(subviews[0]).frame;
  CGRect second_frame = static_cast<UIView*>(subviews[1]).frame;
  if (CGRectGetMinX(first_frame) < CGRectGetMinX(second_frame) ||
      CGRectGetWidth(second_frame) == 0) {
    *leftFrame = first_frame;
    *rightFrame = second_frame;
  } else {
    *rightFrame = first_frame;
    *leftFrame = second_frame;
  }
  return true;
}

}  // namespace

@interface FormInputAccessoryViewController ()

// Allows injection of the JsSuggestionManager.
- (instancetype)initWithWebState:(web::WebState*)webState
             JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager
                       providers:(NSArray*)providers;

// Called when the keyboard will or did change frame.
- (void)keyboardWillOrDidChangeFrame:(NSNotification*)notification;

// Called when the keyboard is dismissed.
- (void)keyboardDidHide:(NSNotification*)notification;

// Hides the subviews in |accessoryView|.
- (void)hideSubviewsInOriginalAccessoryView:(UIView*)accessoryView;

// Attempts to execute/tap/send-an-event-to the iOS built-in "next" and
// "previous" form assist controls. Returns NO if this attempt failed, YES
// otherwise. [HACK]
- (BOOL)executeFormAssistAction:(NSString*)actionName;

// Runs |block| while allowing the keyboard to be displayed as a result of focus
// changes caused by |block|.
- (void)runBlockAllowingKeyboardDisplay:(ProceduralBlock)block;

// Asynchronously retrieves an accessory view from |_providers|.
- (void)retrieveAccessoryViewForForm:(const std::string&)formName
                               field:(const std::string&)fieldName
                               value:(const std::string&)value
                                type:(const std::string&)type
                            webState:(web::WebState*)webState;

// Clears the current custom accessory view and restores the default.
- (void)reset;

// The current web state.
@property(nonatomic, readonly) web::WebState* webState;

// The current web view proxy.
@property(nonatomic, readonly) id<CRWWebViewProxy> webViewProxy;

@end

@implementation FormInputAccessoryViewController {
  // Bridge to observe the web state from Objective-C.
  scoped_ptr<web::WebStateObserverBridge> _webStateObserverBridge;

  // Last registered keyboard rectangle.
  CGRect _keyboardFrame;

  // The custom view that should be shown in the input accessory view.
  base::scoped_nsobject<UIView> _customAccessoryView;

  // The JS manager for interacting with the underlying form.
  base::scoped_nsobject<JsSuggestionManager> _JSSuggestionManager;

  // The original subviews in keyboard accessory view that were originally not
  // hidden but were hidden when showing Autofill suggestions.
  base::scoped_nsobject<NSMutableArray> _hiddenOriginalSubviews;

  // The objects that can provide a custom input accessory view while filling
  // forms.
  base::scoped_nsobject<NSArray> _providers;

  // Whether suggestions have previously been shown.
  BOOL _suggestionsHaveBeenShown;

  // The object that manages the currently-shown custom accessory view.
  base::WeakNSProtocol<id<FormInputAccessoryViewProvider>> _currentProvider;

  // Logs UMA metrics for the keyboard accessory.
  scoped_ptr<autofill::KeyboardAccessoryMetricsLogger>
      _keyboardAccessoryMetricsLogger;
}

- (instancetype)initWithWebState:(web::WebState*)webState
                       providers:(NSArray*)providers {
  JsSuggestionManager* suggestionManager =
      base::mac::ObjCCastStrict<JsSuggestionManager>(
          [webState->GetJSInjectionReceiver()
              instanceOfClass:[JsSuggestionManager class]]);
  return [self initWithWebState:webState
            JSSuggestionManager:suggestionManager
                      providers:providers];
}

- (instancetype)initWithWebState:(web::WebState*)webState
             JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager
                       providers:(NSArray*)providers {
  self = [super init];
  if (self) {
    _JSSuggestionManager.reset([JSSuggestionManager retain]);
    _hiddenOriginalSubviews.reset([[NSMutableArray alloc] init]);
    _webStateObserverBridge.reset(
        new web::WebStateObserverBridge(webState, self));
    _providers.reset([providers copy]);
    _suggestionsHaveBeenShown = NO;
    _keyboardAccessoryMetricsLogger.reset(
        new autofill::KeyboardAccessoryMetricsLogger());
  }
  return self;
}

- (void)wasShown {
  // There is no defined relation on the timing of JavaScript events and
  // keyboard showing up. So it is necessary to listen to the keyboard
  // notification to make sure the keyboard is updated.
  if (base::ios::IsRunningOnIOS9OrLater() && IsIPadIdiom()) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(keyboardWillOrDidChangeFrame:)
               name:UIKeyboardWillChangeFrameNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(textInputDidBeginEditing:)
               name:UITextFieldTextDidBeginEditingNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(textInputDidBeginEditing:)
               name:UITextViewTextDidBeginEditingNotification
             object:nil];
  }
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillOrDidChangeFrame:)
             name:UIKeyboardDidChangeFrameNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidHide:)
             name:UIKeyboardDidHideNotification
           object:nil];
}

- (void)wasHidden {
  [_customAccessoryView removeFromSuperview];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (web::WebState*)webState {
  return _webStateObserverBridge ? _webStateObserverBridge->web_state()
                                 : nullptr;
}

- (id<CRWWebViewProxy>)webViewProxy {
  return self.webState ? self.webState->GetWebViewProxy() : nil;
}

- (void)hideSubviewsInOriginalAccessoryView:(UIView*)accessoryView {
  for (UIView* subview in [accessoryView subviews]) {
    if (!subview.hidden) {
      [_hiddenOriginalSubviews addObject:subview];
      subview.hidden = YES;
    }
  }
}

- (void)showCustomInputAccessoryView:(UIView*)view {
  DCHECK(view);
  if (base::ios::IsRunningOnIOS9OrLater() && IsIPadIdiom()) {
    // On iPads running iOS 9 or later, there's no inputAccessoryView available
    // so we attach the custom view directly to the keyboard view instead.
    [_customAccessoryView removeFromSuperview];

    // If the keyboard isn't visible don't show the custom view.
    if (CGRectIntersection([UIScreen mainScreen].bounds, _keyboardFrame)
                .size.height == 0 ||
        CGRectEqualToRect(_keyboardFrame, CGRectZero)) {
      _customAccessoryView.reset();
      return;
    }

    // If this is a form suggestion view and no suggestions have been triggered
    // yet, don't show the custom view.
    FormSuggestionView* formSuggestionView =
        base::mac::ObjCCastStrict<FormSuggestionView>(view);
    if (formSuggestionView) {
      int numSuggestions = [[formSuggestionView suggestions] count];
      if (!_suggestionsHaveBeenShown && numSuggestions == 0) {
        _customAccessoryView.reset();
        return;
      }
    }
    _suggestionsHaveBeenShown = YES;

    CGFloat height = autofill::kInputAccessoryHeight;
    CGRect contentFrame = self.webViewProxy.frame;
    CGRect frame = CGRectMake(contentFrame.origin.x, -height,
                              contentFrame.size.width, height);
    _customAccessoryView.reset(
        [[FormInputAccessoryView alloc] initWithFrame:frame customView:view]);
    UIView* keyboardView = [self getKeyboardView];
    DCHECK(keyboardView);
    [keyboardView addSubview:_customAccessoryView];
  } else {
    // On all other versions, the custom view replaces the default UI of the
    // inputAccessoryView.
    [self restoreDefaultInputAccessoryView];
    CGRect leftFrame;
    CGRect rightFrame;
    UIView* inputAccessoryView = [self.webViewProxy keyboardAccessory];
    if (ComputeFramesOfKeyboardParts(inputAccessoryView, &leftFrame,
                                     &rightFrame)) {
      [self hideSubviewsInOriginalAccessoryView:inputAccessoryView];
      _customAccessoryView.reset([[FormInputAccessoryView alloc]
          initWithFrame:inputAccessoryView.frame
               delegate:self
             customView:view
              leftFrame:leftFrame
             rightFrame:rightFrame]);
      [inputAccessoryView addSubview:_customAccessoryView];
    }
  }
}

- (void)restoreDefaultInputAccessoryView {
  [_customAccessoryView removeFromSuperview];
  _customAccessoryView.reset();
  for (UIView* subview in _hiddenOriginalSubviews.get()) {
    subview.hidden = NO;
  }
  [_hiddenOriginalSubviews removeAllObjects];
}

- (void)closeKeyboardWithButtonPress {
  [self closeKeyboardWithoutButtonPress];
  if (_currentProvider && [_currentProvider getLogKeyboardAccessoryMetrics])
    _keyboardAccessoryMetricsLogger->OnCloseButtonPressed();
}

- (void)closeKeyboardWithoutButtonPress {
  BOOL performedAction =
      [self executeFormAssistAction:autofill::kFormSuggestionAssistButtonDone];

  if (!performedAction) {
    // We could not find the built-in form assist controls, so try to focus
    // the next or previous control using JavaScript.
    [self runBlockAllowingKeyboardDisplay:^{
      [_JSSuggestionManager closeKeyboard];
    }];
  }
}

- (BOOL)executeFormAssistAction:(NSString*)actionName {
  NSArray* descendants = nil;
  if (base::ios::IsRunningOnIOS9OrLater() && IsIPadIdiom()) {
#if defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0
    UITextInputAssistantItem* inputAssistantItem =
        [self.webViewProxy inputAssistantItem];
    if (!inputAssistantItem)
      return NO;
    descendants =
        FindDescendantToolbarItemsForActionName(inputAssistantItem, actionName);
#endif
  } else {
    UIView* inputAccessoryView = [self.webViewProxy keyboardAccessory];
    if (!inputAccessoryView)
      return NO;
    descendants =
        FindDescendantToolbarItemsForActionName(inputAccessoryView, actionName);
  }

  if (![descendants count])
    return NO;

  UIBarButtonItem* item = descendants[0];
  [[item target] performSelector:[item action] withObject:item];
  return YES;
}

- (void)runBlockAllowingKeyboardDisplay:(ProceduralBlock)block {
  DCHECK([UIWebView
      instancesRespondToSelector:@selector(keyboardDisplayRequiresUserAction)]);

  BOOL originalValue = [self.webViewProxy keyboardDisplayRequiresUserAction];
  [self.webViewProxy setKeyboardDisplayRequiresUserAction:NO];
  block();
  [self.webViewProxy setKeyboardDisplayRequiresUserAction:originalValue];
}

#pragma mark -
#pragma mark FormInputAccessoryViewDelegate

- (void)selectPreviousElementWithButtonPress {
  [self selectPreviousElementWithoutButtonPress];
  if (_currentProvider && [_currentProvider getLogKeyboardAccessoryMetrics])
    _keyboardAccessoryMetricsLogger->OnPreviousButtonPressed();
}

- (void)selectPreviousElementWithoutButtonPress {
  BOOL performedAction =
      [self executeFormAssistAction:
                autofill::kFormSuggestionAssistButtonPreviousElement];
  if (!performedAction) {
    // We could not find the built-in form assist controls, so try to focus
    // the next or previous control using JavaScript.
    [self runBlockAllowingKeyboardDisplay:^{
      [_JSSuggestionManager selectPreviousElement];
    }];
  }
}

- (void)selectNextElementWithButtonPress {
  [self selectNextElementWithoutButtonPress];
  if (_currentProvider && [_currentProvider getLogKeyboardAccessoryMetrics])
    _keyboardAccessoryMetricsLogger->OnNextButtonPressed();
}

- (void)selectNextElementWithoutButtonPress {
  BOOL performedAction = [self
      executeFormAssistAction:autofill::kFormSuggestionAssistButtonNextElement];

  if (!performedAction) {
    // We could not find the built-in form assist controls, so try to focus
    // the next or previous control using JavaScript.
    [self runBlockAllowingKeyboardDisplay:^{
      [_JSSuggestionManager selectNextElement];
    }];
  }
}

- (void)fetchPreviousAndNextElementsPresenceWithCompletionHandler:
        (void (^)(BOOL, BOOL))completionHandler {
  DCHECK(completionHandler);
  [_JSSuggestionManager
      fetchPreviousAndNextElementsPresenceWithCompletionHandler:
          completionHandler];
}

#pragma mark -
#pragma mark CRWWebStateObserver

- (void)webStateDidLoadPage:(web::WebState*)webState {
  [self reset];
}

- (void)webState:(web::WebState*)webState
    didRegisterFormActivityWithFormNamed:(const std::string&)formName
                               fieldName:(const std::string&)fieldName
                                    type:(const std::string&)type
                                   value:(const std::string&)value
                                 keyCode:(int)keyCode
                            inputMissing:(BOOL)inputMissing {
  web::URLVerificationTrustLevel trustLevel;
  const GURL pageURL(webState->GetCurrentURL(&trustLevel));
  if (inputMissing || trustLevel != web::URLVerificationTrustLevel::kAbsolute ||
      !web::UrlHasWebScheme(pageURL) || !webState->ContentIsHTML()) {
    [self reset];
    return;
  }

  if ((type == "blur" || type == "change")) {
    return;
  }

  [self retrieveAccessoryViewForForm:formName
                               field:fieldName
                               value:value
                                type:type
                            webState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  [self reset];
  _webStateObserverBridge.reset();
}

- (void)reset {
  if (_currentProvider) {
    [_currentProvider inputAccessoryViewControllerDidReset:self];
    _currentProvider.reset();
  }
  [self restoreDefaultInputAccessoryView];

  _keyboardAccessoryMetricsLogger.reset(
      new autofill::KeyboardAccessoryMetricsLogger());
}

- (void)retrieveAccessoryViewForForm:(const std::string&)formName
                               field:(const std::string&)fieldName
                               value:(const std::string&)value
                                type:(const std::string&)type
                            webState:(web::WebState*)webState {
  base::WeakNSObject<FormInputAccessoryViewController> weakSelf(self);
  std::string strongFormName = formName;
  std::string strongFieldName = fieldName;
  std::string strongValue = value;
  std::string strongType = type;

  // Build a block for each provider that will invoke its completion with YES
  // if the provider can provide an accessory view for the specified form/field
  // and NO otherwise.
  base::scoped_nsobject<NSMutableArray> findProviderBlocks(
      [[NSMutableArray alloc] init]);
  for (NSUInteger i = 0; i < [_providers count]; i++) {
    base::mac::ScopedBlock<passwords::PipelineBlock> block(
        ^(void (^completion)(BOOL success)) {
          // Access all the providers through |self| to guarantee that both
          // |self| and all the providers exist when the block is executed.
          // |_providers| is immutable, so the subscripting is always valid.
          base::scoped_nsobject<FormInputAccessoryViewController> strongSelf(
              [weakSelf retain]);
          if (!strongSelf)
            return;
          id<FormInputAccessoryViewProvider> provider =
              strongSelf.get()->_providers[i];
          [provider checkIfAccessoryViewIsAvailableForFormNamed:strongFormName
                                                      fieldName:strongFieldName
                                                       webState:webState
                                              completionHandler:completion];
        },
        base::scoped_policy::RETAIN);
    [findProviderBlocks addObject:block];
  }

  // Once the view is retrieved, update the UI.
  AccessoryViewReadyCompletion readyCompletion =
      ^(UIView* accessoryView, id<FormInputAccessoryViewProvider> provider) {
        base::scoped_nsobject<FormInputAccessoryViewController> strongSelf(
            [weakSelf retain]);
        if (!strongSelf || !strongSelf.get()->_currentProvider)
          return;
        DCHECK_EQ(strongSelf.get()->_currentProvider.get(), provider);
        [provider setAccessoryViewDelegate:strongSelf];
        [strongSelf showCustomInputAccessoryView:accessoryView];
      };

  // Once a provider is found, use it to retrieve the accessory view.
  passwords::PipelineCompletionBlock onProviderFound =
      ^(NSUInteger providerIndex) {
        if (providerIndex == NSNotFound) {
          [weakSelf reset];
          return;
        }
        base::scoped_nsobject<FormInputAccessoryViewController> strongSelf(
            [weakSelf retain]);
        if (!strongSelf || ![strongSelf webState])
          return;
        id<FormInputAccessoryViewProvider> provider =
            strongSelf.get()->_providers[providerIndex];
        [strongSelf.get()->_currentProvider
            inputAccessoryViewControllerDidReset:self];
        strongSelf.get()->_currentProvider.reset(provider);
        [strongSelf.get()->_currentProvider
            retrieveAccessoryViewForFormNamed:strongFormName
                                    fieldName:strongFieldName
                                        value:strongValue
                                         type:strongType
                                     webState:webState
                     accessoryViewUpdateBlock:readyCompletion];
      };

  // Run all the blocks in |findProviderBlocks| until one invokes its
  // completion with YES. The first one to do so will be passed to
  // |onProviderFound|.
  passwords::RunSearchPipeline(findProviderBlocks, onProviderFound);
}

- (UIView*)getKeyboardView {
  NSArray* windows = [UIApplication sharedApplication].windows;
  if (windows.count < 2)
    return nil;

  UIWindow* window = windows[1];
  for (UIView* subview in window.subviews) {
    if ([NSStringFromClass([subview class]) rangeOfString:@"PeripheralHost"]
            .location != NSNotFound) {
      return subview;
    }
    if ([NSStringFromClass([subview class]) rangeOfString:@"SetContainer"]
            .location != NSNotFound) {
      for (UIView* subsubview in subview.subviews) {
        if ([NSStringFromClass([subsubview class]) rangeOfString:@"SetHost"]
                .location != NSNotFound) {
          return subsubview;
        }
      }
    }
  }

  return nil;
}

- (void)keyboardWillOrDidChangeFrame:(NSNotification*)notification {
  if (!self.webState || !_currentProvider)
    return;
  CGRect keyboardFrame =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  // With iOS8 (beta) this method can be called even when the rect has not
  // changed. When this is detected we exit early.
  if (CGRectEqualToRect(CGRectIntegral(_keyboardFrame),
                        CGRectIntegral(keyboardFrame))) {
    return;
  }
  _keyboardFrame = keyboardFrame;
  [_currentProvider resizeAccessoryView];
}

// On iPads running iOS 9 or later, when any text field or text view (e.g.
// omnibox, settings, card unmask dialog) begins editing, reset ourselves so
// that we don't present our custom view over the keyboard.
- (void)textInputDidBeginEditing:(NSNotification*)notification {
  [self reset];
}

- (void)keyboardDidHide:(NSNotification*)notification {
  _keyboardFrame = CGRectZero;
}

@end
