// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator.h"

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_updater.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"
#import "ios/chrome/browser/ui/ntp/ntp_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_visibility_configuration.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_coordinator_delegate.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_mediator.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tools_menu_button.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#import "ios/chrome/browser/ui/toolbar/tools_menu_button_observer_bridge.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_player.h"
#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ToolbarCoordinator ()<OmniboxPopupPositioner> {
  // Observer that updates |toolbarViewController| for fullscreen events.
  std::unique_ptr<FullscreenControllerObserver> _fullscreenObserver;
}

// The View Controller managed by this coordinator.
@property(nonatomic, strong) ToolbarViewController* toolbarViewController;
// The mediator owned by this coordinator.
@property(nonatomic, strong) ToolbarMediator* mediator;
// Whether this coordinator has been started.
@property(nonatomic, assign) BOOL started;
// Button observer for the ToolsMenu button.
@property(nonatomic, strong)
    ToolsMenuButtonObserverBridge* toolsMenuButtonObserverBridge;
// The coordinator for the location bar in the toolbar.
@property(nonatomic, strong) LocationBarCoordinator* locationBarCoordinator;

@end

@implementation ToolbarCoordinator
@synthesize delegate = _delegate;
@synthesize browserState = _browserState;
@synthesize buttonUpdater = _buttonUpdater;
@synthesize dispatcher = _dispatcher;
@synthesize mediator = _mediator;
@synthesize started = _started;
@synthesize toolbarViewController = _toolbarViewController;
@synthesize toolsMenuButtonObserverBridge = _toolsMenuButtonObserverBridge;
@synthesize URLLoader = _URLLoader;
@synthesize webStateList = _webStateList;
@synthesize locationBarCoordinator = _locationBarCoordinator;

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

  self.locationBarCoordinator = [[LocationBarCoordinator alloc] init];
  self.locationBarCoordinator.browserState = self.browserState;
  self.locationBarCoordinator.dispatcher = self.dispatcher;
  self.locationBarCoordinator.URLLoader = self.URLLoader;
  self.locationBarCoordinator.delegate = self.delegate;
  self.locationBarCoordinator.webStateList = self.webStateList;
  self.locationBarCoordinator.popupPositioner = self;
  [self.locationBarCoordinator start];

  ToolbarStyle style = isIncognito ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:style];
  factory.dispatcher = self.dispatcher;
  factory.visibilityConfiguration =
      [[ToolbarButtonVisibilityConfiguration alloc] initWithType:LEGACY];

  self.buttonUpdater = [[ToolbarButtonUpdater alloc] init];
  self.buttonUpdater.factory = factory;
  self.toolbarViewController = [[ToolbarViewController alloc]
      initWithDispatcher:self.dispatcher
           buttonFactory:factory
           buttonUpdater:self.buttonUpdater
          omniboxFocuser:self.locationBarCoordinator];
  self.toolbarViewController.locationBarView =
      self.locationBarCoordinator.locationBarView;
  self.toolbarViewController.dispatcher = self.dispatcher;

  if (base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen)) {
    _fullscreenObserver =
        std::make_unique<FullscreenUIUpdater>(self.toolbarViewController);
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
  if (!self.started)
    return;

  self.started = NO;
  self.delegate = nil;
  [self.mediator disconnect];
  [self.locationBarCoordinator stop];
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

- (id<OmniboxFocuser>)omniboxFocuser {
  return self.locationBarCoordinator;
}

- (void)updateToolbarForSideSwipeSnapshot:(web::WebState*)webState {
  BOOL isNTP = IsVisibleUrlNewTabPage(webState);

  // Don't do anything for a live non-ntp tab.
  if (webState == self.webStateList->GetActiveWebState() && !isNTP) {
    [self.locationBarCoordinator.locationBarView setHidden:NO];
    return;
  }

  self.viewController.view.hidden = NO;
  [self.locationBarCoordinator.locationBarView setHidden:YES];
  [self.mediator updateConsumerForWebState:webState];
  [self.toolbarViewController updateForSideSwipeSnapshotOnNTP:isNTP];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [self.mediator
      updateConsumerForWebState:self.webStateList->GetActiveWebState()];
  [self.locationBarCoordinator.locationBarView setHidden:NO];
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

- (BOOL)isOmniboxFirstResponder {
  return
      [self.locationBarCoordinator.locationBarView.textField isFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  return [self.locationBarCoordinator showingOmniboxPopup];
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

- (UIView*)popupParentView {
  return self.toolbarViewController.view.superview;
}

#pragma mark - LocationBarDelegate

- (void)transitionToLocationBarFocusedState:(BOOL)focused {
  if (IsIPadIdiom()) {
    [self.toolbarViewController locationBarIsFirstResonderOnIPad:focused];
    return;
  }

  DCHECK(!IsIPadIdiom());
  if (focused == self.toolbarViewController.expanded) {
    // The view controller is already in the correct state.
    return;
  }

  if (focused) {
    [self expandOmniboxAnimated:YES];
  } else {
    [self contractOmnibox];
  }
}


#pragma mark - FakeboxFocuser

- (void)focusFakebox {
  if (IsIPadIdiom()) {
    // On iPhone there is no visible omnibox, so there's no need to indicate
    // interaction was initiated from the fakebox.
    [self.locationBarCoordinator focusOmniboxFromFakebox];
  } else {
    [self expandOmniboxAnimated:NO];
  }

  [self.locationBarCoordinator focusOmnibox];

  if ([self.locationBarCoordinator omniboxPopupHasAutocompleteResults]) {
    [self onFakeboxAnimationComplete];
  }
}

- (void)onFakeboxBlur {
  DCHECK(!IsIPadIdiom());
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = self.webStateList->GetActiveWebState();
  if (webState && IsVisibleUrlNewTabPage(webState)) {
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
    [self.locationBarCoordinator focusOmnibox];
    [self.locationBarCoordinator.locationBarView.textField
        insertTextWhileEditing:result];
    // The call to |setText| shouldn't be needed, but without it the "Go" button
    // of the keyboard is disabled.
    [self.locationBarCoordinator.locationBarView.textField setText:result];
    // Notify the accessibility system to start reading the new contents of the
    // Omnibox.
    UIAccessibilityPostNotification(
        UIAccessibilityScreenChangedNotification,
        self.locationBarCoordinator.locationBarView.textField);
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
            animations:nil];
  UIViewPropertyAnimator* completionAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:ios::material::kDuration1
                 curve:UIViewAnimationCurveEaseOut
            animations:nil];
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    [completionAnimator startAnimationAfterDelay:ios::material::kDuration4];
  }];

  [self.locationBarCoordinator.locationBarView
      addExpandOmniboxAnimations:animator
              completionAnimator:completionAnimator];
  [self.toolbarViewController addToolbarExpansionAnimations:animator
                                         completionAnimator:completionAnimator];
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
  [self.locationBarCoordinator.locationBarView
      addContractOmniboxAnimations:animator];
  [self.toolbarViewController addToolbarContractionAnimations:animator];
  [animator startAnimation];
}

@end
