// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"

#import <CoreLocation/CoreLocation.h>

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/search_engines/util.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/toolbar/tools_menu_button_observer_bridge.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_player.h"
#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<LocationBarDelegate, OmniboxPopupPositioner> {
  std::unique_ptr<LocationBarControllerImpl> _locationBar;
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
  // Observer that updates |toolbarViewController| for fullscreen events.
  std::unique_ptr<FullscreenControllerObserver> _fullscreenObserver;
}

// The View Controller managed by this coordinator.
@property(nonatomic, strong) ToolbarViewController* toolbarViewController;
// The mediator owned by this coordinator.
@property(nonatomic, strong) ToolbarMediator* mediator;
// LocationBarView containing the omnibox. At some point, this property and the
// |_locationBar| should become a LocationBarCoordinator.
@property(nonatomic, strong) LocationBarView* locationBarView;
// Object taking care of adding the accessory views to the keyboard.
@property(nonatomic, strong)
    ToolbarAssistiveKeyboardDelegateImpl* keyboardDelegate;
// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Button observer for the ToolsMenu button.
@property(nonatomic, strong)
    ToolsMenuButtonObserverBridge* toolsMenuButtonObserverBridge;

@end

@implementation ToolbarCoordinator
@synthesize delegate = _delegate;
@synthesize browserState = _browserState;
@synthesize buttonUpdater = _buttonUpdater;
@synthesize dispatcher = _dispatcher;
@synthesize keyboardDelegate = _keyboardDelegate;
@synthesize locationBarView = _locationBarView;
@synthesize mediator = _mediator;
@synthesize started = _started;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize toolsMenuButtonObserverBridge = _toolsMenuButtonObserverBridge;
@synthesize URLLoader = _URLLoader;
@synthesize webStateList = _webStateList;

- (instancetype)init {
  if ((self = [super init])) {
    _mediator = [[ToolbarMediator alloc] init];
  }
  return self;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return self.toolbarViewController;
}

- (void)setDelegate:(id<ToolbarCoordinatorDelegate>)delegate {
  if (_delegate == delegate)
    return;

  // TTS notifications cannot be handled without a delegate.
  if (_delegate && self.started)
    [self stopObservingTTSNotifications];
  _delegate = delegate;
  if (_delegate && self.started)
    [self startObservingTTSNotifications];
}

#pragma mark - BrowserCoordinator

- (void)start {
  if (self.started)
    return;

  self.started = YES;
  BOOL isIncognito = self.browserState->IsOffTheRecord();
  // TODO(crbug.com/785253): Move this to the LocationBarCoordinator once it is
  // created.
  UIColor* textColor =
      isIncognito
          ? [UIColor whiteColor]
          : [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
  UIColor* tintColor = isIncognito ? textColor : nil;
  self.locationBarView =
      [[LocationBarView alloc] initWithFrame:CGRectZero
                                        font:[MDCTypography subheadFont]
                                   textColor:textColor
                                   tintColor:tintColor];
  _locationBar = base::MakeUnique<LocationBarControllerImpl>(
      self.locationBarView, self.browserState, self, self.dispatcher);
  self.locationBarView.incognito = isIncognito;
  self.locationBarView.textField.incognito = isIncognito;
  if (isIncognito) {
    [_locationBarView.textField
        setSelectedTextBackgroundColor:[UIColor colorWithWhite:1 alpha:0.1]];
    [_locationBarView.textField
        setPlaceholderTextColor:[UIColor colorWithWhite:1 alpha:0.5]];
  } else if (!IsIPadIdiom()) {
    // Set placeholder text color to match fakebox placeholder text color when
    // on iPhone.
    UIColor* placeholderTextColor =
        [UIColor colorWithWhite:kiPhoneOmniboxPlaceholderColorBrightness
                          alpha:1.0];
    [_locationBarView.textField setPlaceholderTextColor:placeholderTextColor];
  }

  self.keyboardDelegate = [[ToolbarAssistiveKeyboardDelegateImpl alloc] init];
  self.keyboardDelegate.dispatcher = self.dispatcher;
  self.keyboardDelegate.omniboxTextField = self.locationBarView.textField;
  ConfigureAssistiveKeyboardViews(self.locationBarView.textField, kDotComTLD,
                                  self.keyboardDelegate);

  _popupView = _locationBar->CreatePopupView(self);
  // End of TODO(crbug.com/785253):.

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];

  self.buttonUpdater = [[ToolbarButtonUpdater alloc] init];
  self.buttonUpdater.factory = factory;
  self.toolbarViewController =
      [[ToolbarViewController alloc] initWithDispatcher:self.dispatcher
                                          buttonFactory:factory
                                          buttonUpdater:self.buttonUpdater
                                         omniboxFocuser:self];
  self.toolbarViewController.locationBarView = self.locationBarView;
  self.toolbarViewController.dispatcher = self.dispatcher;

  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen)) {
    _fullscreenObserver =
        base::MakeUnique<FullscreenUIUpdater>(self.toolbarViewController);
    FullscreenControllerFactory::GetInstance()
        ->GetForBrowserState(self.browserState)
        ->AddObserver(_fullscreenObserver.get());
  }

  DCHECK(self.toolbarViewController.toolsMenuButton);
  self.toolsMenuButtonObserverBridge = [[ToolsMenuButtonObserverBridge alloc]
      initWithModel:ReadingListModelFactory::GetForBrowserState(
                        self.browserState)
      toolbarButton:self.toolbarViewController.toolsMenuButton];

  self.mediator.voiceSearchProvider =
      ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
  self.mediator.consumer = self.toolbarViewController;
  self.mediator.webStateList = self.webStateList;
  self.mediator.bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(self.browserState);

  if (self.delegate)
    [self startObservingTTSNotifications];
}

- (void)stop {
  self.started = NO;
  [self.mediator disconnect];
  // The popup has to be destroyed before the location bar.
  _popupView.reset();
  _locationBar.reset();
  self.locationBarView = nil;
  [self stopObservingTTSNotifications];

  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen)) {
    FullscreenControllerFactory::GetInstance()
        ->GetForBrowserState(self.browserState)
        ->RemoveObserver(_fullscreenObserver.get());
    _fullscreenObserver = nullptr;
  }
}

#pragma mark - Public

- (id<ActivityServicePositioner>)activityServicePositioner {
  return self.toolbarViewController;
}

- (id<BubbleViewAnchorPointProvider>)bubbleAnchorPointProvider {
  return self.toolbarViewController;
}

- (void)updateOmniboxState {
  _locationBar->SetShouldShowHintText(
      [self.delegate toolbarModelIOS]->ShouldDisplayHintText());
  _locationBar->OnToolbarUpdated();
}

- (void)updateToolbarState {
  // TODO(crbug.com/784911): This function should probably triggers something in
  // the mediator. Investigate how to handle it.
  [self updateOmniboxState];
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  web::NavigationItem* item =
      webState->GetNavigationManager()->GetVisibleItem();
  GURL URL = item ? item->GetURL().GetOrigin() : GURL::EmptyGURL();
  BOOL isNTP = (URL == GURL(kChromeUINewTabURL));

  // Don't do anything for a live non-ntp tab.
  if (webState == [self getWebState] && !isNTP) {
    [_locationBarView setHidden:NO];
    return;
  }

  self.viewController.view.hidden = NO;
  [_locationBarView setHidden:YES];
  [self.mediator updateConsumerForWebState:webState];
  [self.toolbarViewController updateForSideSwipeSnapshotOnNTP:isNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator updateConsumerForWebState:[self getWebState]];
  [_locationBarView setHidden:NO];
  [self.toolbarViewController resetAfterSideSwipeSnapshot];
}

- (void)setToolsMenuIsVisibleForToolsMenuButton:(BOOL)isVisible {
  [self.toolbarViewController.toolsMenuButton setToolsMenuIsVisible:isVisible];
}

- (void)triggerToolsMenuButtonAnimation {
  [self.toolbarViewController.toolsMenuButton triggerAnimation];
}

- (void)setBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha {
  [self.toolbarViewController setBackgroundToIncognitoNTPColorWithAlpha:alpha];
}

- (void)showPrerenderingAnimation {
  [self.toolbarViewController showPrerenderingAnimation];
}

- (CGRect)visibleOmniboxFrame {
  return [self.toolbarViewController visibleOmniboxFrame];
}

- (BOOL)isOmniboxFirstResponder {
  return [_locationBarView.textField isFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  OmniboxViewIOS* omniboxViewIOS =
      static_cast<OmniboxViewIOS*>(_locationBar.get()->GetLocationEntry());
  return omniboxViewIOS->IsPopupOpen();
}

- (void)activateFakeSafeAreaInsets:(UIEdgeInsets)fakeSafeAreaInsets {
  [self.toolbarViewController activateFakeSafeAreaInsets:fakeSafeAreaInsets];
}

- (void)deactivateFakeSafeAreaInsets {
  [self.toolbarViewController deactivateFakeSafeAreaInsets];
}

// TODO(crbug.com/786940): This protocol should move to the ViewController
// owning the Toolbar. This can wait until the omnibox and toolbar refactoring
// is more advanced.
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupAnchorView {
  return self.toolbarViewController.view;
}

- (CGRect)popupFrame:(CGFloat)height {
  UIView* parent = [[self popupAnchorView] superview];
  CGRect frame = [parent bounds];

  if (IsIPadIdiom()) {
    // For iPad, the omnibox is displayed between the location bar and the
    // bottom of the toolbar.
    CGRect fieldFrame = [parent convertRect:[_locationBarView bounds]
                                   fromView:_locationBarView];
    CGFloat maxY = CGRectGetMaxY(fieldFrame);
    frame.origin.y = maxY + kiPadOmniboxPopupVerticalOffset;
    frame.size.height = height;
  } else {
    // For iPhone place the popup just below the toolbar.
    CGRect fieldFrame =
        [parent convertRect:[self.toolbarViewController.view bounds]
                   fromView:self.toolbarViewController.view];
    frame.origin.y = CGRectGetMaxY(fieldFrame);
    frame.size.height = CGRectGetMaxY([parent bounds]) - frame.origin.y;
  }
  return frame;
}

#pragma mark - LocationBarDelegate

- (void)loadGURLFromLocationBar:(const GURL&)url
                     transition:(ui::PageTransition)transition {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Evaluate the URL as JavaScript if its scheme is JavaScript.
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [self.URLLoader loadJavaScriptFromLocationBar:jsToEval];
  } else {
    // When opening a URL, force the omnibox to resign first responder.  This
    // will also close the popup.

    // TODO(crbug.com/785244): Is it ok to call |cancelOmniboxEdit| after
    // |loadURL|?  It doesn't seem to be causing major problems.  If we call
    // cancel before load, then any prerendered pages get destroyed before the
    // call to load.
    [self.URLLoader loadURL:url
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];

    if (google_util::IsGoogleSearchUrl(url)) {
      UMA_HISTOGRAM_ENUMERATION(
          kOmniboxQueryLocationAuthorizationStatusHistogram,
          [CLLocationManager authorizationStatus],
          kLocationAuthorizationStatusCount);
    }
  }
  [self cancelOmniboxEdit];
}

- (void)locationBarHasBecomeFirstResponder {
  [self.delegate locationBarDidBecomeFirstResponder];
  if (IsIPadIdiom()) {
    [self.toolbarViewController locationBarIsFirstResonderOnIPad:YES];
  } else if (!self.toolbarViewController.expanded) {
    [self expandOmniboxAnimated:YES];
  }
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
  if (IsIPadIdiom()) {
    [self.toolbarViewController locationBarIsFirstResonderOnIPad:NO];
  } else if (self.toolbarViewController.expanded) {
    [self contractOmnibox];
  }
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)getWebState {
  return self.webStateList->GetActiveWebState();
}

- (ToolbarModel*)toolbarModel {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  return toolbarModelIOS ? toolbarModelIOS->GetToolbarModel() : nullptr;
}

#pragma mark - OmniboxFocuser

- (void)focusOmnibox {
  [_locationBarView.textField becomeFirstResponder];
}

- (void)cancelOmniboxEdit {
  _locationBar->HideKeyboardAndEndEditing();
  [self updateToolbarState];
}

- (void)focusFakebox {
  if (IsIPadIdiom()) {
    OmniboxEditModel* model = _locationBar->GetLocationEntry()->model();
    // Setting the caret visibility to false causes OmniboxEditModel to indicate
    // that omnibox interaction was initiated from the fakebox. Note that
    // SetCaretVisibility is a no-op unless OnSetFocus is called first.  Only
    // set fakebox on iPad, where there is a distinction between the omnibox
    // and the fakebox on the NTP.  On iPhone there is no visible omnibox, so
    // there's no need to indicate interaction was initiated from the fakebox.
    model->OnSetFocus(false);
    model->SetCaretVisibility(false);
  } else {
    [self expandOmniboxAnimated:NO];
  }

  [self focusOmnibox];
}

- (void)onFakeboxBlur {
  DCHECK(!IsIPadIdiom());
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = [self getWebState];
  if (webState && (webState->GetVisibleURL() == GURL(kChromeUINewTabURL))) {
    self.viewController.view.hidden = YES;
  }
}

- (void)onFakeboxAnimationComplete {
  DCHECK(!IsIPadIdiom());
  self.viewController.view.hidden = NO;
}

#pragma mark - VoiceSearchControllerDelegate

- (void)receiveVoiceSearchResult:(NSString*)result {
  DCHECK(result);
  [self loadURLForQuery:result];
}

#pragma mark - QRScannerResultLoading

- (void)receiveQRScannerResult:(NSString*)result loadImmediately:(BOOL)load {
  DCHECK(result);
  if (load) {
    [self loadURLForQuery:result];
  } else {
    [self focusOmnibox];
    [_locationBarView.textField insertTextWhileEditing:result];
    // The call to |setText| shouldn't be needed, but without it the "Go" button
    // of the keyboard is disabled.
    [_locationBarView.textField setText:result];
    // Notify the accessibility system to start reading the new contents of the
    // Omnibox.
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _locationBarView.textField);
  }
}

#pragma mark - ToolsMenuPresentationProvider

- (UIButton*)presentingButtonForToolsMenuCoordinator:
    (ToolsMenuCoordinator*)coordinator {
  return self.toolbarViewController.toolsMenuButton;
}

#pragma mark - TTS

// Starts observing the NSNotifications from the Text-To-Speech player.
- (void)startObservingTTSNotifications {
  // The toolbar is only used to play text-to-speech search results on iPads.
  if (IsIPadIdiom()) {
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(audioReadyForPlayback:)
                          name:kTTSAudioReadyForPlaybackNotification
                        object:nil];
    const auto& selectorsForTTSNotifications =
        [[self class] selectorsForTTSNotificationNames];
    for (const auto& selectorForNotification : selectorsForTTSNotifications) {
      [defaultCenter addObserver:self.buttonUpdater
                        selector:selectorForNotification.second
                            name:selectorForNotification.first
                          object:nil];
    }
  }
}

// Stops observing the NSNotifications from the Text-To-Speech player.
- (void)stopObservingTTSNotifications {
  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter removeObserver:self
                           name:kTTSAudioReadyForPlaybackNotification
                         object:nil];
  const auto& selectorsForTTSNotifications =
      [[self class] selectorsForTTSNotificationNames];
  for (const auto& selectorForNotification : selectorsForTTSNotifications) {
    [defaultCenter removeObserver:self.buttonUpdater
                             name:selectorForNotification.first
                           object:nil];
  }
}

// Returns a map where the keys are names of text-to-speech notifications and
// the values are the selectors to use for these notifications.
+ (const std::map<__strong NSString*, SEL>&)selectorsForTTSNotificationNames {
  static std::map<__strong NSString*, SEL> selectorsForNotifications;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    selectorsForNotifications[kTTSWillStartPlayingNotification] =
        @selector(updateIsTTSPlaying:);
    selectorsForNotifications[kTTSDidStopPlayingNotification] =
        @selector(updateIsTTSPlaying:);
    selectorsForNotifications[kVoiceSearchWillHideNotification] =
        @selector(moveVoiceOverToVoiceSearchButton);
  });
  return selectorsForNotifications;
}

// Received when a TTS player has received audio data.
- (void)audioReadyForPlayback:(NSNotification*)notification {
  if ([self.buttonUpdater canStartPlayingTTS]) {
    // Only trigger TTS playback when the voice search button is visible.
    TextToSpeechPlayer* TTSPlayer =
        base::mac::ObjCCastStrict<TextToSpeechPlayer>(notification.object);
    [TTSPlayer beginPlayback];
  }
}

#pragma mark - Private

// Navigate to |query| from omnibox.
- (void)loadURLForQuery:(NSString*)query {
  GURL searchURL;
  metrics::OmniboxInputType type = AutocompleteInput::Parse(
      base::SysNSStringToUTF16(query), std::string(),
      AutocompleteSchemeClassifierImpl(), nullptr, nullptr, &searchURL);
  if (type != metrics::OmniboxInputType::URL || !searchURL.is_valid()) {
    searchURL = GetDefaultSearchURLForSearchTerms(
        ios::TemplateURLServiceFactory::GetForBrowserState(self.browserState),
        base::SysNSStringToUTF16(query));
  }
  if (searchURL.is_valid()) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [self.URLLoader loadURL:GURL(searchURL)
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];
  }
}

// Animates |_toolbar| and |_locationBarView| for omnibox expansion. If
// |animated| is NO the animation will happen instantly.
- (void)expandOmniboxAnimated:(BOOL)animated {
  // iPad should never try to expand.
  DCHECK(!IsIPadIdiom());

  UIViewPropertyAnimator* animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:ios::material::kDuration1
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
            }];

  [self.locationBarView addExpandOmniboxAnimations:animator];
  [self.toolbarViewController addToolbarExpansionAnimations:animator];
  [animator startAnimation];

  if (!animated) {
    [animator stopAnimation:NO];
    [animator finishAnimationAtPosition:UIViewAnimatingPositionEnd];
  }
}

// Animates |_toolbar| and |_locationBarView| for omnibox contraction.
- (void)contractOmnibox {
  // iPad should never try to contract.
  DCHECK(!IsIPadIdiom());

  UIViewPropertyAnimator* animator = [[UIViewPropertyAnimator alloc]
      initWithDuration:ios::material::kDuration1
                 curve:UIViewAnimationCurveEaseInOut
            animations:^{
            }];
  [self.locationBarView addContractOmniboxAnimations:animator];
  [self.toolbarViewController addToolbarContractionAnimations:animator];
  [animator startAnimation];
}

@end
