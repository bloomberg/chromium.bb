// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"

#import <CoreLocation/CoreLocation.h>
#include <QuartzCore/QuartzCore.h>

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/drag_and_drop/drag_and_drop_flag.h"
#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_delegate.h"
#include "ios/chrome/browser/drag_and_drop/drop_and_navigate_interaction.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/animation_util.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/external_search_commands.h"
#import "ios/chrome/browser/ui/commands/start_voice_search_command.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/image_util.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_controller_impl.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_delegate.h"
#include "ios/chrome/browser/ui/omnibox/location_bar_view.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_view.h"
#import "ios/chrome/browser/ui/reversed_animation.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_factory.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_button_updater.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_delegate.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/toolbar/public/web_toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller+protected.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_model_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_resource_macros.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_view_delegate.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_delegate.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/browser/ui/voice/text_to_speech_player.h"
#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"
#import "ios/chrome/common/material_timing.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/web_state.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;
using ios::material::TimingFunction;

// TODO(crbug.com/619982) Remove this block and add CAAnimationDelegate when we
// switch the main bots to Xcode 8.
#if defined(__IPHONE_10_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0)
@interface WebToolbarController ()<CAAnimationDelegate>
@property(nonatomic, weak) id<UrlLoader> urlLoader;
@end
#endif

@interface WebToolbarController ()<DropAndNavigateDelegate,
                                   LocationBarDelegate,
                                   OmniboxPopupPositioner,
                                   ToolbarViewDelegate> {
  // Top-level view for web content.
  UIView* _webToolbar;
  UIButton* _backButton;
  UIButton* _forwardButton;
  UIButton* _reloadButton;
  UIButton* _stopButton;
  UIButton* _starButton;
  UIButton* _voiceSearchButton;
  LocationBarView* _locationBarView;
  UIButton* _cancelButton;
  // Progress bar used to show what fraction of the page has loaded.
  MDCProgressView* _determinateProgressView;
  UIImageView* _omniboxBackground;
  BOOL _prerenderAnimating;
  UIImageView* _incognitoIcon;
  UIView* _clippingView;

  std::unique_ptr<LocationBarControllerImpl> _locationBar;
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
  BOOL _initialLayoutComplete;
  // If |YES|, toolbar is incognito.
  BOOL _incognito;

  // If set to |YES|, disables animations that tests would otherwise trigger.
  BOOL _unitTesting;

  // Keeps track of the last known toolbar Y origin.
  CGFloat _lastKnownToolbarYOrigin;

  // Keeps track of last known trait collection used by the subviews.
  UITraitCollection* _lastKnownTraitCollection;

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
  API_AVAILABLE(ios(11.0)) DropAndNavigateInteraction* _dropInteraction;
#endif

  // The current browser state.
  ios::ChromeBrowserState* _browserState;  // weak

  ToolbarAssistiveKeyboardDelegateImpl* _keyboardDelegate;
}

// Accessor for cancel button. Handles lazy initialization.
- (UIButton*)cancelButton;
// Handler called after user pressed the cancel button.
- (void)cancelButtonPressed:(id)sender;
- (void)layoutCancelButton;
// Change the location bar dimensions according to the focus status.
// Also show/hide relevant buttons.
- (void)layoutOmnibox;
- (void)setBackButtonEnabled:(BOOL)enabled;
// Show or hide the forward button, animating the frame of the location bar to
// be in the right position. This can be called multiple times.
- (void)setForwardButtonEnabled:(BOOL)enabled;
- (void)startProgressBar;
- (void)stopProgressBar;
- (void)hideProgressBar;
- (void)showReloadButton;
- (void)showStopButton;
// Called by long press gesture recognizer, used to display back/forward
// history.
- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture;
// Returns a map where the keys are names of text-to-speech notifications and
// the values are the selectors to use for these notifications.
+ (const std::map<__strong NSString*, SEL>&)selectorsForTTSNotificationNames;
// Starts or stops observing the NSNotifications from
// |-selectorsForTTSNotificationNames|.
- (void)startObservingTTSNotifications;
- (void)stopObservingTTSNotifications;
// Received when a TTS player has received audio data.
- (void)audioReadyForPlayback:(NSNotification*)notification;
// Fade in and out toolbar items as the frame moves off screen.
- (void)updateToolbarAlphaForFrame:(CGRect)frame;
// Navigate to |query| from omnibox.
- (void)loadURLForQuery:(NSString*)query;
- (void)preloadVoiceSearch:(id)sender;
// Calculates the CGRect to use for the omnibox's frame. Also sets the frames
// of some buttons and |_webToolbar|.
- (CGRect)newOmniboxFrame;
- (void)animateMaterialOmnibox;
- (void)fadeInOmniboxTrailingView;
- (void)fadeInOmniboxLeadingView;
- (void)fadeOutOmniboxTrailingView;
- (void)fadeOutOmniboxLeadingView;
- (void)fadeInIncognitoIcon;
- (void)fadeOutIncognitoIcon;
// Fade in the visible navigation buttons.
- (void)fadeInNavigationControls;
// Fade out the visible navigation buttons.
- (void)fadeOutNavigationControls;
// When the collapse animation is complete, hide the Material background and
// restore the omnibox's background image.
- (void)animationDidStop:(CAAnimation*)anim finished:(BOOL)flag;
// Updates all buttons visibility, including the parent class buttons.
- (void)updateToolbarButtons;

// Redifined as readwrite
@property(nonatomic, strong, readwrite) ToolbarButtonUpdater* buttonUpdater;

@end

@implementation WebToolbarController

@synthesize buttonUpdater = _buttonUpdater;
@synthesize delegate = _delegate;
@synthesize urlLoader = _urlLoader;

- (instancetype)initWithDelegate:(id<WebToolbarDelegate>)delegate
                       urlLoader:(id<UrlLoader>)urlLoader
                    browserState:(ios::ChromeBrowserState*)browserState
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher {
  DCHECK(delegate);
  DCHECK(urlLoader);
  DCHECK(browserState);
  _delegate = delegate;
  _urlLoader = urlLoader;
  _browserState = browserState;
  _incognito = browserState->IsOffTheRecord();
  ToolbarControllerStyle style =
      (_incognito ? ToolbarControllerStyleIncognitoMode
                  : ToolbarControllerStyleLightMode);
  self = [super initWithStyle:style dispatcher:dispatcher];
  if (!self)
    return nil;

  self.readingListModel =
      ReadingListModelFactory::GetForBrowserState(browserState);

  InterfaceIdiom idiom = IsIPadIdiom() ? IPAD_IDIOM : IPHONE_IDIOM;
  // Note that |_webToolbar| gets its frame set to -specificControlArea later in
  // this method.
  _webToolbar =
      [[UIView alloc] initWithFrame:LayoutRectGetRect(kWebToolbarFrame[idiom])];
  UIColor* textColor =
      _incognito
          ? [UIColor whiteColor]
          : [UIColor colorWithWhite:0 alpha:[MDCTypography body1FontOpacity]];
  UIColor* tintColor = _incognito ? textColor : nil;
  CGRect omniboxRect = LayoutRectGetRect(kOmniboxFrame[idiom]);
  _locationBarView =
      [[LocationBarView alloc] initWithFrame:omniboxRect
                                        font:[MDCTypography subheadFont]
                                   textColor:textColor
                                   tintColor:tintColor];
  _locationBarView.clipsToBounds = YES;
  _keyboardDelegate = [[ToolbarAssistiveKeyboardDelegateImpl alloc] init];
  _keyboardDelegate.dispatcher = dispatcher;
  _keyboardDelegate.omniboxTextField = _locationBarView.textField;

  // Disable default drop interactions on the omnibox.
  // TODO(crbug.com/739903): Handle drop events once Chrome iOS is built with
  // the iOS 11 SDK.
  if (base::ios::IsRunningOnIOS11OrLater()) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    SEL setInteractionsSelector = NSSelectorFromString(@"setInteractions:");
    if ([_locationBarView.textField
            respondsToSelector:setInteractionsSelector]) {
      [_locationBarView.textField performSelector:setInteractionsSelector
                                       withObject:@[]];
    }
#pragma clang diagnostic pop
  }
  if (_incognito) {
    [_locationBarView setIncognito:YES];
    [_locationBarView.textField setIncognito:YES];
    [_locationBarView.textField
        setSelectedTextBackgroundColor:[UIColor colorWithWhite:1 alpha:0.1]];
    [_locationBarView.textField
        setPlaceholderTextColor:[UIColor colorWithWhite:1 alpha:0.5]];
  } else if (!IsIPadIdiom()) {
    // Set placeholder text color to match fakebox placeholder text color when
    // on iPhone and in regular mode.
    UIColor* placeholderTextColor =
        [UIColor colorWithWhite:kiPhoneOmniboxPlaceholderColorBrightness
                          alpha:1.0];
    [_locationBarView.textField setPlaceholderTextColor:placeholderTextColor];
  }

  ToolbarStyle incognitoStyle =
      _browserState->IsOffTheRecord() ? INCOGNITO : NORMAL;
  ToolbarButtonFactory* factory =
      [[ToolbarButtonFactory alloc] initWithStyle:incognitoStyle];
  _buttonUpdater = [[ToolbarButtonUpdater alloc] init];
  _buttonUpdater.factory = factory;

  _backButton = [[UIButton alloc]
      initWithFrame:LayoutRectGetRect(kBackButtonFrame[idiom])];
  _buttonUpdater.backButton = _backButton;
  [_backButton setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                                   UIViewAutoresizingFlexibleTopMargin];

  // Note that the forward button gets repositioned when -layoutOmnibox is
  // called.
  _forwardButton = [[UIButton alloc]
      initWithFrame:LayoutRectGetRect(kForwardButtonFrame[idiom])];
  _buttonUpdater.forwardButton = _forwardButton;
  [_forwardButton
      setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                          UIViewAutoresizingFlexibleTopMargin];

  [_webToolbar addSubview:_backButton];
  [_webToolbar addSubview:_forwardButton];

  // _omniboxBackground needs to be added under _locationBarView so as not to
  // cover up _locationBarView.
  _omniboxBackground = [[UIImageView alloc] initWithFrame:omniboxRect];
  [_omniboxBackground setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                          UIViewAutoresizingFlexibleTopMargin];

  if (idiom == IPAD_IDIOM) {
    [_webToolbar addSubview:_omniboxBackground];
  } else {
    [_backButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, 0, 0, -9)];
    [_forwardButton setImageEdgeInsets:UIEdgeInsetsMakeDirected(0, -7, 0, 0)];
    CGRect clippingFrame =
        RectShiftedUpAndResizedForStatusBar(kToolbarFrame[idiom]);
    _clippingView = [[UIView alloc] initWithFrame:clippingFrame];
    [_clippingView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                       UIViewAutoresizingFlexibleHeight];
    [_clippingView setClipsToBounds:YES];
    [_clippingView setUserInteractionEnabled:NO];
    [_webToolbar addSubview:_clippingView];

    CGRect omniboxBackgroundFrame = RectShiftedDownForStatusBar(omniboxRect);
    [_omniboxBackground setFrame:omniboxBackgroundFrame];
    [_clippingView addSubview:_omniboxBackground];
    [self.view
        setBackgroundColor:[UIColor colorWithWhite:kNTPBackgroundColorBrightness
                                             alpha:1.0]];

    if (_incognito) {
      [self.view
          setBackgroundColor:
              [UIColor colorWithWhite:kNTPBackgroundColorBrightnessIncognito
                                alpha:1.0]];
      _incognitoIcon = [[UIImageView alloc]
          initWithImage:[UIImage imageNamed:@"incognito_marker_typing"]];
      [_incognitoIcon setAlpha:0];
      [_incognitoIcon
          setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin()];
      [self layoutIncognitoIcon];
      [_webToolbar addSubview:_incognitoIcon];
    }
  }

  [_webToolbar addSubview:_locationBarView];

  [_backButton setEnabled:NO];
  [_forwardButton setEnabled:NO];

  if (idiom == IPAD_IDIOM) {
    // Note that the reload button gets repositioned when -layoutOmnibox is
    // called.
    _reloadButton = [[UIButton alloc]
        initWithFrame:LayoutRectGetRect(kStopReloadButtonFrame)];
    [_reloadButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                            UIViewAutoresizingFlexibleTopMargin];
    _stopButton = [[UIButton alloc]
        initWithFrame:LayoutRectGetRect(kStopReloadButtonFrame)];
    [_stopButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTrailingMargin() |
                            UIViewAutoresizingFlexibleTopMargin];
    _starButton =
        [[UIButton alloc] initWithFrame:LayoutRectGetRect(kStarButtonFrame)];
    [_starButton setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                                     UIViewAutoresizingFlexibleLeadingMargin()];
    _voiceSearchButton = [[UIButton alloc]
        initWithFrame:LayoutRectGetRect(kVoiceSearchButtonFrame)];
    _buttonUpdater.voiceSearchButton = _voiceSearchButton;
    [_voiceSearchButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                            UIViewAutoresizingFlexibleLeadingMargin()];
    [_voiceSearchButton addTarget:self
                           action:@selector(toolbarVoiceSearchButtonPressed:)
                 forControlEvents:UIControlEventTouchUpInside];

    [_webToolbar addSubview:_voiceSearchButton];
    [_webToolbar addSubview:_starButton];
    [_webToolbar addSubview:_stopButton];
    [_webToolbar addSubview:_reloadButton];
    [self setUpButton:_voiceSearchButton
           withImageEnum:WebToolbarButtonNameVoice
         forInitialState:UIControlStateNormal
        hasDisabledImage:NO
           synchronously:NO];
    [self setUpButton:_starButton
           withImageEnum:WebToolbarButtonNameStar
         forInitialState:UIControlStateNormal
        hasDisabledImage:NO
           synchronously:YES];
    [self setUpButton:_stopButton
           withImageEnum:WebToolbarButtonNameStop
         forInitialState:UIControlStateDisabled
        hasDisabledImage:YES
           synchronously:NO];
    [self setUpButton:_reloadButton
           withImageEnum:WebToolbarButtonNameReload
         forInitialState:UIControlStateNormal
        hasDisabledImage:YES
           synchronously:NO];
    [_stopButton setHidden:YES];

    // Assign targets for buttons using the dispatcher.
    [_stopButton addTarget:self.dispatcher
                    action:@selector(stopLoading)
          forControlEvents:UIControlEventTouchUpInside];
    [_reloadButton addTarget:self.dispatcher
                      action:@selector(reload)
            forControlEvents:UIControlEventTouchUpInside];
    [_starButton addTarget:self.dispatcher
                    action:@selector(bookmarkPage)
          forControlEvents:UIControlEventTouchUpInside];
  } else {
    [_forwardButton setAlpha:0.0];
  }

  // Set up the button images and omnibox background.
  [self setUpButton:_backButton
         withImageEnum:WebToolbarButtonNameBack
       forInitialState:UIControlStateDisabled
      hasDisabledImage:YES
         synchronously:NO];
  [self setUpButton:_forwardButton
         withImageEnum:WebToolbarButtonNameForward
       forInitialState:UIControlStateDisabled
      hasDisabledImage:YES
         synchronously:NO];

  // Assign targets for buttons using the dispatcher.
  [_backButton addTarget:self.dispatcher
                  action:@selector(goBack)
        forControlEvents:UIControlEventTouchUpInside];
  [_forwardButton addTarget:self.dispatcher
                     action:@selector(goForward)
           forControlEvents:UIControlEventTouchUpInside];

  UILongPressGestureRecognizer* backLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [_backButton addGestureRecognizer:backLongPress];
  UILongPressGestureRecognizer* forwardLongPress =
      [[UILongPressGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(handleLongPress:)];
  [_forwardButton addGestureRecognizer:forwardLongPress];

  // TODO(leng):  Consider moving this to a pak file as well.  For now,
  // because it is also used by find_bar_controller_ios, leave it as is.
  NSString* imageName =
      _incognito ? @"omnibox_transparent_background" : @"omnibox_background";
  [_omniboxBackground setImage:StretchableImageNamed(imageName, 12, 12)];
  [_locationBarView setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                        UIViewAutoresizingFlexibleTopMargin];
  [_reloadButton addTarget:self
                    action:@selector(cancelOmniboxEdit)
          forControlEvents:UIControlEventTouchUpInside];
  [_stopButton addTarget:self
                  action:@selector(cancelOmniboxEdit)
        forControlEvents:UIControlEventTouchUpInside];

  SetA11yLabelAndUiAutomationName(_backButton, IDS_ACCNAME_BACK, @"Back");
  SetA11yLabelAndUiAutomationName(_forwardButton, IDS_ACCNAME_FORWARD,
                                  @"Forward");
  SetA11yLabelAndUiAutomationName(_reloadButton, IDS_IOS_ACCNAME_RELOAD,
                                  @"Reload");
  SetA11yLabelAndUiAutomationName(_stopButton, IDS_IOS_ACCNAME_STOP, @"Stop");
  SetA11yLabelAndUiAutomationName(_starButton, IDS_TOOLTIP_STAR, @"Bookmark");
  SetA11yLabelAndUiAutomationName(
      _voiceSearchButton, IDS_IOS_ACCNAME_VOICE_SEARCH, @"Voice Search");
  SetA11yLabelAndUiAutomationName(_locationBarView.textField,
                                  IDS_ACCNAME_LOCATION, @"Address");

  // Resize the container to match the available area.
  [self.contentView addSubview:_webToolbar];
  [_webToolbar setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                   UIViewAutoresizingFlexibleTopMargin];
  [_webToolbar setFrame:[self specificControlsArea]];
  _locationBar = base::MakeUnique<LocationBarControllerImpl>(
      _locationBarView, _browserState, self, self.dispatcher);
  _popupView = _locationBar->CreatePopupView(self);

  // Create the determinate progress bar (phone only).
  if (idiom == IPHONE_IDIOM) {
    CGFloat progressWidth = self.view.frame.size.width;
    CGFloat progressHeight = 0;
    progressHeight = kMaterialProgressBarHeight;
    _determinateProgressView = [[MDCProgressView alloc] init];
    _determinateProgressView.hidden = YES;
    [_determinateProgressView
        setProgressTintColor:[MDCPalette cr_bluePalette].tint500];
    [_determinateProgressView
        setTrackTintColor:[MDCPalette cr_bluePalette].tint100];
    int progressBarYOffset =
        floor(progressHeight / 2) + kDeterminateProgressBarYOffset;
    int progressBarY = self.view.bounds.size.height - progressBarYOffset;
    CGRect progressBarFrame =
        CGRectMake(0, progressBarY, progressWidth, progressHeight);
    [_determinateProgressView setFrame:progressBarFrame];
    [self.view addSubview:_determinateProgressView];
  }

  ConfigureAssistiveKeyboardViews(_locationBarView.textField, kDotComTLD,
                                  _keyboardDelegate);

  // Add the handler to preload voice search when the voice search button is
  // tapped, but only if voice search is enabled.
  if (ios::GetChromeBrowserProvider()
          ->GetVoiceSearchProvider()
          ->IsVoiceSearchEnabled()) {
    [_voiceSearchButton addTarget:self
                           action:@selector(preloadVoiceSearch:)
                 forControlEvents:UIControlEventTouchDown];
  } else {
    [_voiceSearchButton setEnabled:NO];
  }

  [self startObservingTTSNotifications];

  if (!base::FeatureList::IsEnabled(fullscreen::features::kNewFullscreen))
    [self.view setDelegate:self];

  if (idiom == IPHONE_IDIOM) {
    [[self stackButton] addTarget:dispatcher
                           action:@selector(displayTabSwitcher)
                 forControlEvents:UIControlEventTouchUpInside];
  }

#if defined(__IPHONE_11_0) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
  if (DragAndDropIsEnabled()) {
    if (@available(iOS 11, *)) {
      _dropInteraction =
          [[DropAndNavigateInteraction alloc] initWithDelegate:self];
      [self.view addInteraction:_dropInteraction];
    }
  }
#endif

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (UIViewController*)viewController {
  return self;
}

- (void)start {
}

#pragma mark -
#pragma mark Acessors

- (void)setDelegate:(id<WebToolbarDelegate>)delegate {
  if (_delegate == delegate)
    return;

  // TTS notifications cannot be handled without a delegate.
  if (_delegate)
    [self stopObservingTTSNotifications];
  _delegate = delegate;
  if (_delegate)
    [self startObservingTTSNotifications];
}

#pragma mark -
#pragma mark Public methods.

- (void)browserStateDestroyed {
  // The location bar has a browser state reference, so must be destroyed at
  // this point. The popup has to be destroyed before the location bar.
  _popupView.reset();
  _locationBar.reset();
  _browserState = nullptr;
}

- (void)updateToolbarState {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  if (toolbarModelIOS->IsLoading()) {
    [self showStopButton];
    [self startProgressBar];
    [_determinateProgressView
        setProgress:toolbarModelIOS->GetLoadProgressFraction()
           animated:YES
         completion:nil];
  } else {
    [self stopProgressBar];
    [self showReloadButton];
  }

  _locationBar->SetShouldShowHintText(toolbarModelIOS->ShouldDisplayHintText());
  _locationBar->OnToolbarUpdated();
  BOOL forwardButtonEnabled = toolbarModelIOS->CanGoForward();
  [self setBackButtonEnabled:toolbarModelIOS->CanGoBack()];
  [self setForwardButtonEnabled:forwardButtonEnabled];

  // Update the bookmarked "starred" state of current tab.
  [_starButton setSelected:toolbarModelIOS->IsCurrentTabBookmarked()];

  if (!_initialLayoutComplete)
    _initialLayoutComplete = YES;
}

- (void)updateToolbarForSideSwipeSnapshot:(Tab*)tab {
  web::WebState* webState = tab.webState;
  BOOL isCurrentTab = webState == [self.delegate currentWebState];
  BOOL isNTP = webState->GetVisibleURL() == GURL(kChromeUINewTabURL);

  // Don't do anything for a live non-ntp tab.
  if (isCurrentTab && !isNTP) {
    // This has the effect of making any alpha-ed out items visible.
    [self updateToolbarAlphaForFrame:CGRectZero];
    [_locationBarView setHidden:NO];
    return;
  }

  [[self view] setHidden:NO];
  [_determinateProgressView setHidden:YES];
  BOOL forwardEnabled = tab.canGoForward;
  [_backButton setHidden:isNTP ? !forwardEnabled : NO];
  [_backButton setEnabled:tab.canGoBack];
  [_forwardButton setHidden:!forwardEnabled];
  [_forwardButton setEnabled:forwardEnabled];
  [_locationBarView setHidden:YES];
  [self.backgroundView setAlpha:isNTP ? 0 : 1];
  [_omniboxBackground setHidden:isNTP ? YES : NO];
  [self hideViewsForNewTabPage:isNTP ? YES : NO];
  [self layoutOmnibox];
}

- (void)resetToolbarAfterSideSwipeSnapshot {
  [_locationBarView setHidden:NO];
  [_backButton setHidden:NO];
  [_forwardButton setHidden:NO];
  [_omniboxBackground setHidden:NO];
  [self.backgroundView setAlpha:1];
  [self hideViewsForNewTabPage:NO];
  [self updateToolbarState];
}

- (void)showPrerenderingAnimation {
  _prerenderAnimating = YES;
  __weak MDCProgressView* weakDeterminateProgressView =
      _determinateProgressView;
  [_determinateProgressView setProgress:0];
  [_determinateProgressView setHidden:NO
                             animated:YES
                           completion:^(BOOL finished) {
                             [weakDeterminateProgressView
                                 setProgress:1
                                    animated:YES
                                  completion:^(BOOL finished) {
                                    [weakDeterminateProgressView setHidden:YES
                                                                  animated:YES
                                                                completion:nil];
                                  }];
                           }];
}

- (void)currentPageLoadStarted {
  [self startProgressBar];
}

- (CGRect)visibleOmniboxFrame {
  CGRect frame = _omniboxBackground.frame;
  frame = [self.view.superview convertRect:frame
                                  fromView:[_omniboxBackground superview]];
  // Account for the omnibox background image transparent sides.
  return CGRectInset(frame, -kBackgroundImageVisibleRectOffset, 0);
}

- (BOOL)isOmniboxFirstResponder {
  return [_locationBarView.textField isFirstResponder];
}

- (BOOL)showingOmniboxPopup {
  OmniboxViewIOS* omniboxViewIOS =
      static_cast<OmniboxViewIOS*>(_locationBar.get()->GetLocationEntry());
  return omniboxViewIOS->IsPopupOpen();
}

#pragma mark -
#pragma mark Overridden public superclass methods.

- (void)layoutClippingView {
  CGRect clippingFrame = [_clippingView frame];
  clippingFrame.size.height =
      ToolbarHeightWithTopOfScreenOffset(StatusBarHeight());
  clippingFrame.origin.y =
      [_webToolbar frame].size.height - clippingFrame.size.height;
  [_clippingView setFrame:clippingFrame];

  // Omnibox background height also must be manually resized.
  if ([_locationBarView.textField isFirstResponder]) {
    CGRect omniboxFrame = CGRectInset([_clippingView bounds], -2, -2);
    [_omniboxBackground setFrame:omniboxFrame];
  }
}

- (void)setUpButton:(UIButton*)button
       withImageEnum:(int)imageEnum
     forInitialState:(UIControlState)initialState
    hasDisabledImage:(BOOL)hasDisabledImage
       synchronously:(BOOL)synchronously {
  [super setUpButton:button
         withImageEnum:imageEnum
       forInitialState:initialState
      hasDisabledImage:hasDisabledImage
         synchronously:synchronously];

  if (button != _starButton)
    return;
  // The star button behaves slightly differently.  It uses the pressed
  // image for its selected state as well as its pressed state.
  void (^starBlock)(void) = ^{
    UIImage* starImage = [self imageForImageEnum:WebToolbarButtonNameStar
                                        forState:ToolbarButtonUIStatePressed];
    [_starButton setAdjustsImageWhenHighlighted:NO];
    [_starButton setImage:starImage forState:UIControlStateSelected];
  };
  if (synchronously) {
    starBlock();
  } else {
    dispatch_time_t addImageDelay =
        dispatch_time(DISPATCH_TIME_NOW, kNonInitialImageAdditionDelayNanosec);
    dispatch_after(addImageDelay, dispatch_get_main_queue(), starBlock);
  }
}

- (BOOL)imageShouldFlipForRightToLeftLayoutDirection:(int)imageEnum {
  DCHECK(imageEnum < NumberOfWebToolbarButtonNames);
  if (imageEnum < NumberOfToolbarButtonNames)
    return [super imageShouldFlipForRightToLeftLayoutDirection:imageEnum];
  if (imageEnum == WebToolbarButtonNameBack ||
      imageEnum == WebToolbarButtonNameForward ||
      imageEnum == WebToolbarButtonNameReload ||
      imageEnum == WebToolbarButtonNameCallingApp) {
    return YES;
  }
  return NO;
}

- (void)hideViewsForNewTabPage:(BOOL)hide {
  [super hideViewsForNewTabPage:hide];
  if (_incognito) {
    CGFloat alpha = hide ? 0 : 1;
    [self.backgroundView setAlpha:alpha];
  }
}

#pragma mark -
#pragma mark Overridden protected superclass methods.

- (void)standardButtonPressed:(UIButton*)sender {
  [super standardButtonPressed:sender];
  [self cancelOmniboxEdit];
}

- (IBAction)recordUserMetrics:(id)sender {
  if (sender == _backButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarBack"));
  } else if (sender == _forwardButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarForward"));
  } else if (sender == _reloadButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarReload"));
  } else if (sender == _stopButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarStop"));
  } else if (sender == _voiceSearchButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarVoiceSearch"));
  } else if (sender == _starButton) {
    base::RecordAction(UserMetricsAction("MobileToolbarToggleBookmark"));
  } else {
    [super recordUserMetrics:sender];
  }
}

- (int)imageEnumForButton:(UIButton*)button {
  if (button == _voiceSearchButton)
    return self.buttonUpdater.TTSPlaying ? WebToolbarButtonNameTTS
                                         : WebToolbarButtonNameVoice;
  if (button == _starButton)
    return WebToolbarButtonNameStar;
  if (button == _stopButton)
    return WebToolbarButtonNameStop;
  if (button == _reloadButton)
    return WebToolbarButtonNameReload;
  if (button == _backButton)
    return WebToolbarButtonNameBack;
  if (button == _forwardButton)
    return WebToolbarButtonNameForward;
  return [super imageEnumForButton:button];
}

- (int)imageIdForImageEnum:(int)index
                     style:(ToolbarControllerStyle)style
                  forState:(ToolbarButtonUIState)state {
  DCHECK(style < ToolbarControllerStyleMaxStyles);
  DCHECK(state < NumberOfToolbarButtonUIStates);
  // Additional checking.  These three buttons should only ever be used on iPad.
  DCHECK(IsIPadIdiom() || index != WebToolbarButtonNameStar);
  DCHECK(IsIPadIdiom() || index != WebToolbarButtonNameTTS);
  DCHECK(IsIPadIdiom() || index != WebToolbarButtonNameVoice);

  if (index >= NumberOfWebToolbarButtonNames)
    NOTREACHED();
  if (index < NumberOfToolbarButtonNames)
    return [super imageIdForImageEnum:index style:style forState:state];

  // Incognito mode gets dark buttons.
  if (style == ToolbarControllerStyleIncognitoMode)
    style = ToolbarControllerStyleDarkMode;

  // Some images can be overridden by the branded image provider.
  if (index == WebToolbarButtonNameVoice) {
    int image_id;
    if (ios::GetChromeBrowserProvider()
            ->GetBrandedImageProvider()
            ->GetToolbarVoiceSearchButtonImageId(&image_id)) {
      return image_id;
    }
    // Otherwise fall through to the default voice search button images below.
  }

  // Rebase |index| so that it properly indexes into the array below.
  index -= NumberOfToolbarButtonNames;
  const int numberOfAddedNames =
      NumberOfWebToolbarButtonNames - NumberOfToolbarButtonNames;
  // Name, style [light, dark], UIControlState [normal, pressed, disabled]
  static int buttonImageIds[numberOfAddedNames][2]
                           [NumberOfToolbarButtonUIStates] = {
                               TOOLBAR_IDR_THREE_STATE(BACK),
                               TOOLBAR_IDR_LIGHT_ONLY_TWO_STATE(CALLINGAPP),
                               TOOLBAR_IDR_THREE_STATE(FORWARD),
                               TOOLBAR_IDR_THREE_STATE(RELOAD),
                               TOOLBAR_IDR_TWO_STATE(STAR),
                               TOOLBAR_IDR_THREE_STATE(STOP),
                               TOOLBAR_IDR_TWO_STATE(TTS),
                               TOOLBAR_IDR_TWO_STATE(VOICE),
                           };
  return buttonImageIds[index][style][state];
}

#pragma mark -
#pragma mark LocationBarDelegate methods.

- (void)loadGURLFromLocationBar:(const GURL&)url
                     transition:(ui::PageTransition)transition {
  if (url.SchemeIs(url::kJavaScriptScheme)) {
    // Evaluate the URL as JavaScript if its scheme is JavaScript.
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [self.urlLoader loadJavaScriptFromLocationBar:jsToEval];
  } else {
    // When opening a URL, force the omnibox to resign first responder.  This
    // will also close the popup.

    // TODO(rohitrao): Is it ok to call |cancelOmniboxEdit| after |loadURL|?  It
    // doesn't seem to be causing major problems.  If we call cancel before
    // load, then any prerendered pages get destroyed before the call to load.
    [self.urlLoader loadURL:url
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
  [self animateMaterialOmnibox];
}

- (void)locationBarHasResignedFirstResponder {
  [self.delegate locationBarDidResignFirstResponder];
  [self animateMaterialOmnibox];
}

- (void)locationBarBeganEdit {
  [self.delegate locationBarBeganEdit];
}

- (web::WebState*)getWebState {
  return [self.delegate currentWebState];
}

- (ToolbarModel*)toolbarModel {
  ToolbarModelIOS* toolbarModelIOS = [self.delegate toolbarModelIOS];
  return toolbarModelIOS ? toolbarModelIOS->GetToolbarModel() : nullptr;
}

#pragma mark -
#pragma mark OmniboxFocuser methods.

- (void)focusOmnibox {
  if (![_webToolbar isHidden])
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
    // Set the omnibox background's frame to full bleed.
    CGRect mobFrame = CGRectInset([_clippingView bounds], -2, -2);
    [_omniboxBackground setFrame:mobFrame];
  }

  [self focusOmnibox];
}

- (void)onFakeboxBlur {
  DCHECK(!IsIPadIdiom());
  // Hide the toolbar if the NTP is currently displayed.
  web::WebState* webState = [self.delegate currentWebState];
  if (webState && (webState->GetVisibleURL() == GURL(kChromeUINewTabURL))) {
    [self.view setHidden:YES];
  }
}

- (void)onFakeboxAnimationComplete {
  DCHECK(!IsIPadIdiom());
  [self.view setHidden:NO];
}

#pragma mark -
#pragma mark OmniboxPopupPositioner methods.

- (UIView*)popupAnchorView {
  return self.view;
}

- (CGRect)popupFrame:(CGFloat)height {
  UIView* parent = [[self popupAnchorView] superview];
  CGRect frame = [parent bounds];

  // Set tablet popup width to the same width and origin of omnibox.
  if (IsIPadIdiom()) {
    // For iPad, the omnibox visually extends to include the voice search button
    // on the right. Start with the field's frame in |parent|'s coordinate
    // system.
    CGRect fieldFrame = [parent convertRect:[_locationBarView bounds]
                                   fromView:_locationBarView];

    // Now create a new frame that's below the field, stretching the full width
    // of |parent|, minus an inset on each side.
    CGFloat maxY = CGRectGetMaxY(fieldFrame);

    // The popup extends to the full width of the screen.
    frame.origin.x = 0;
    frame.size.width = self.view.frame.size.width;

    frame.origin.y = maxY + kiPadOmniboxPopupVerticalOffset;
    frame.size.height = height;
  } else {
    // For iPhone place the popup just below the toolbar.
    CGRect fieldFrame =
        [parent convertRect:[_webToolbar bounds] fromView:_webToolbar];
    frame.origin.y = CGRectGetMaxY(fieldFrame);
    frame.size.height = CGRectGetMaxY([parent bounds]) - frame.origin.y;
  }
  return frame;
}

#pragma mark -
#pragma mark ToolbarViewDelegate methods.

- (void)toolbarDidLayout {
  CGRect frame = self.view.frame;
  if (frame.origin.y == _lastKnownToolbarYOrigin)
    return;
  [self updateToolbarAlphaForFrame:frame];
  _lastKnownToolbarYOrigin = frame.origin.y;
}

- (void)windowDidChange {
  if (![_lastKnownTraitCollection
          containsTraitsInCollection:self.view.traitCollection]) {
    [self updateToolbarButtons];
  }
}

- (void)traitCollectionDidChange {
  [self updateToolbarButtons];
}

#pragma mark -
#pragma mark VoiceSearchControllerDelegate methods.

- (void)receiveVoiceSearchResult:(NSString*)result {
  DCHECK(result);
  [self loadURLForQuery:result];
}

#pragma mark -
#pragma mark QRScanner Requirements.

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

#pragma mark -
#pragma mark DropAndNavigateDelegate

- (void)URLWasDropped:(GURL const&)gurl {
  ui::PageTransition transition =
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  [self.urlLoader loadURL:gurl
                 referrer:web::Referrer()
               transition:transition
        rendererInitiated:NO];
}

#pragma mark -
#pragma mark CAAnimationDelegate
- (void)animationDidStop:(CAAnimation*)anim finished:(BOOL)flag {
  if ([[anim valueForKey:@"id"] isEqual:@"resizeOmnibox"] &&
      ![_locationBarView.textField isFirstResponder]) {
    [_locationBarView.textField setRightView:nil];
  }
}

#pragma mark -
#pragma mark Private methods.

#pragma mark Layout.

- (void)layoutIncognitoIcon {
  LayoutRect iconLayout = LayoutRectForRectInBoundingRect(
      [_incognitoIcon frame], [_webToolbar bounds]);
  iconLayout.position.leading = 16;
  iconLayout.position.originY = floor(
      (kToolbarFrame[IPHONE_IDIOM].size.height - iconLayout.size.height) / 2);
  [_incognitoIcon setFrame:LayoutRectGetRect(iconLayout)];
}

- (void)layoutOmnibox {
  // Position the forward and reload buttons trailing the back button in that
  // order.
  LayoutRect leadingControlLayout = LayoutRectForRectInBoundingRect(
      [_backButton frame], [_webToolbar bounds]);
  LayoutRect forwardButtonLayout =
      LayoutRectGetTrailingLayout(leadingControlLayout);
  forwardButtonLayout.position.originY = [_forwardButton frame].origin.y;
  forwardButtonLayout.size = [_forwardButton frame].size;

  LayoutRect reloadButtonLayout =
      LayoutRectGetTrailingLayout(forwardButtonLayout);
  reloadButtonLayout.position.originY = [_reloadButton frame].origin.y;
  reloadButtonLayout.size = [_reloadButton frame].size;

  CGRect newForwardButtonFrame = LayoutRectGetRect(forwardButtonLayout);
  CGRect newReloadButtonFrame = LayoutRectGetRect(reloadButtonLayout);
  CGRect newOmniboxFrame = [self newOmniboxFrame];
  BOOL isPad = IsIPadIdiom();
  BOOL growOmnibox = [_locationBarView.textField isFirstResponder];

  // Animate buttons. Hide most of the buttons (standard set, back, forward)
  // for extended omnibox layout. Also show an extra cancel button so the
  // extended mode can be cancelled.
  CGFloat alphaForNormalOmniboxButton = growOmnibox ? 0.0 : 1.0;
  CGFloat alphaForGrownOmniboxButton = growOmnibox ? 1.0 : 0.0;
  CGFloat forwardButtonAlpha =
      [_forwardButton isEnabled] || (isPad && !IsCompactTablet(self.view))
          ? alphaForNormalOmniboxButton
          : 0.0;
  CGFloat backButtonAlpha = alphaForNormalOmniboxButton;
  // For the initial layout/testing we just set the length of animation to 0.0
  // which means the changes will be done without any animation.
  [UIView animateWithDuration:((_initialLayoutComplete && !_unitTesting) ? 0.2
                                                                         : 0.0)
                        delay:0.0
                      options:UIViewAnimationOptionAllowUserInteraction |
                              UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     if (!isPad)
                       [self setStandardControlsVisible:!growOmnibox];
                     if (!(isPad && growOmnibox)) {
                       [_backButton setAlpha:backButtonAlpha];
                       [_forwardButton setAlpha:forwardButtonAlpha];
                       [_forwardButton setFrame:newForwardButtonFrame];
                       [_cancelButton setAlpha:alphaForGrownOmniboxButton];
                       [_reloadButton setFrame:newReloadButtonFrame];
                       [_stopButton setFrame:newReloadButtonFrame];
                     }
                   }
                   completion:nil];

  if (CGRectEqualToRect([_locationBarView frame], newOmniboxFrame))
    return;

  // Hide the clear and voice search buttons during omniBox frame animations.
  [_locationBarView.textField setRightViewMode:UITextFieldViewModeNever];

  CGRect materialBackgroundFrame = RectShiftedDownForStatusBar(newOmniboxFrame);

  // Extreme jank happens during initial layout if an animation is invoked. Not
  // certain why. o_O
  // Duration set to 0.0 prevents the animation during initial layout.
  [UIView animateWithDuration:((_initialLayoutComplete && !_unitTesting) ? 0.2
                                                                         : 0.0)
      delay:0.0
      options:UIViewAnimationOptionAllowUserInteraction
      animations:^{
        [_locationBarView setFrame:newOmniboxFrame];
        [_omniboxBackground setFrame:materialBackgroundFrame];
      }
      completion:^(BOOL finished) {
        [_locationBarView.textField setRightViewMode:UITextFieldViewModeAlways];
      }];
}

// TODO(crbug.com/525943): refactor this method and related code to not resize
// |_webToolbar| and buttons as a side effect.
- (CGRect)newOmniboxFrame {
  InterfaceIdiom idiom = IsIPadIdiom() ? IPAD_IDIOM : IPHONE_IDIOM;
  LayoutRect newOmniboxLayout;
  // Grow the omnibox if focused.
  BOOL growOmnibox = [_locationBarView.textField isFirstResponder];
  if (idiom == IPAD_IDIOM) {
    // When the omnibox is focused, the star button is hidden.
    [_starButton setAlpha:(growOmnibox ? 0 : 1)];

    newOmniboxLayout = LayoutRectForRectInBoundingRect([_locationBarView frame],
                                                       [_webToolbar bounds]);
    CGFloat omniboxLeading = [self omniboxLeading];
    CGFloat omniboxLeadingDiff =
        omniboxLeading - newOmniboxLayout.position.leading;

    newOmniboxLayout.position.leading = omniboxLeading;
    newOmniboxLayout.size.width -= omniboxLeadingDiff;
  } else {
    // If the omnibox is growing, take over the whole toolbar area (standard
    // toolbar controls will be hidden below). Temporarily suppress autoresizing
    // to avoid interfering with the omnibox animation.
    [_webToolbar setAutoresizesSubviews:NO];
    CGRect expandedFrame =
        RectShiftedDownAndResizedForStatusBar(self.contentView.bounds);
    // Using -RectShiftedDownAndResizedForStatusBar can sometimes be wrong in
    // an autolayout world, such as if the view is rotated with the app
    // backgrounded. It's safer to use -SafeAreaInsetsForView.
    if (IsSafeAreaCompatibleToolbarEnabled()) {
      UIEdgeInsets safeAreaInsets = SafeAreaInsetsForView(self.contentView);
      expandedFrame =
          UIEdgeInsetsInsetRect(self.contentView.bounds, safeAreaInsets);
    }
    [_webToolbar
        setFrame:growOmnibox ? expandedFrame : [self specificControlsArea]];
    [_webToolbar setAutoresizesSubviews:YES];

    // Compute new omnibox layout after the web toolbar is resized.
    newOmniboxLayout = LayoutRectForRectInBoundingRect([_locationBarView frame],
                                                       [_webToolbar bounds]);

    if (growOmnibox) {
      // If the omnibox is expanded, there is padding on both the left and right
      // sides.
      newOmniboxLayout.position.leading = kExpandedOmniboxPadding;
      // When the |_webToolbar| is resized without autoresizes subviews enabled
      // the cancel button does not pin to the trailing side and is out of
      // place. Lazily allocate and force a layout here.
      [self layoutCancelButton];
      LayoutRect cancelButtonLayout = LayoutRectForRectInBoundingRect(
          [self cancelButton].frame, [_webToolbar bounds]);
      newOmniboxLayout.size.width = cancelButtonLayout.position.leading -
                                    kExpandedOmniboxPadding -
                                    newOmniboxLayout.position.leading;
      // Likewise the incognito icon will not be pinned to the leading side, so
      // force a layout here.
      if (_incognitoIcon)
        [self layoutIncognitoIcon];
    } else {
      newOmniboxLayout.position.leading = [self omniboxLeading];
      newOmniboxLayout.size.width = CGRectGetWidth([_webToolbar frame]) -
                                    newOmniboxLayout.position.leading;
    }
  }

  CGRect newOmniboxFrame = LayoutRectGetRect(newOmniboxLayout);
  DCHECK(!CGRectEqualToRect(newOmniboxFrame, CGRectZero));

  return newOmniboxFrame;
}

- (CGFloat)omniboxLeading {
  // Compute what the leading (x-origin) position for the omniboox should be
  // based on what other controls are active.
  InterfaceIdiom idiom = IsIPadIdiom() ? IPAD_IDIOM : IPHONE_IDIOM;

  CGFloat trailingPadding = 0.0;

  LayoutRect leadingControlLayout = LayoutRectForRectInBoundingRect(
      [_backButton frame], [_webToolbar bounds]);
  LayoutRect forwardButtonLayout =
      LayoutRectGetTrailingLayout(leadingControlLayout);
  forwardButtonLayout.position.leading += trailingPadding;
  forwardButtonLayout.position.originY = [_forwardButton frame].origin.y;
  forwardButtonLayout.size = [_forwardButton frame].size;

  LayoutRect reloadButtonLayout =
      LayoutRectGetTrailingLayout(forwardButtonLayout);
  reloadButtonLayout.position.originY = [_reloadButton frame].origin.y;
  reloadButtonLayout.size = [_reloadButton frame].size;

  LayoutRect omniboxReferenceLayout;
  if (idiom == IPAD_IDIOM && !IsCompactTablet(self.view)) {
    omniboxReferenceLayout = reloadButtonLayout;
    omniboxReferenceLayout.size.width += kReloadButtonTrailingPadding;
  } else if ([_forwardButton isEnabled]) {
    omniboxReferenceLayout = forwardButtonLayout;
    omniboxReferenceLayout.size.width += kForwardButtonTrailingPadding;
  } else {
    omniboxReferenceLayout = kBackButtonFrame[idiom];
    omniboxReferenceLayout.size.width += kBackButtonTrailingPadding;
  }
  DCHECK(!(omniboxReferenceLayout.position.leading == 0 &&
           CGSizeEqualToSize(omniboxReferenceLayout.size, CGSizeZero)));

  return LayoutRectGetTrailingEdge(omniboxReferenceLayout);
}

#pragma mark Toolbar Appearance.

- (void)setBackButtonEnabled:(BOOL)enabled {
  [_backButton setEnabled:enabled];
}

- (void)setForwardButtonEnabled:(BOOL)enabled {
  if (enabled == [_forwardButton isEnabled])
    return;

  [_forwardButton setEnabled:enabled];
  if (!enabled && [_forwardButton isHidden])
    return;
  [self layoutOmnibox];
}

- (void)startProgressBar {
  if ([_determinateProgressView isHidden]) {
    [_determinateProgressView setProgress:0];
    [_determinateProgressView setHidden:NO animated:YES completion:nil];
  }
}

- (void)stopProgressBar {
  if (_determinateProgressView && ![_determinateProgressView isHidden]) {
    // Update the toolbar snapshot, but only after the progress bar has
    // disappeared.

    if (!_prerenderAnimating) {
      __weak MDCProgressView* weakDeterminateProgressView =
          _determinateProgressView;
      // Calling -completeAndHide while a prerender animation is in progress
      // will result in hiding the progress bar before the animation is
      // complete.
      [_determinateProgressView setProgress:1
                                   animated:YES
                                 completion:^(BOOL finished) {
                                   [weakDeterminateProgressView setHidden:YES
                                                                 animated:YES
                                                               completion:nil];
                                 }];
    }
    CGFloat delay = _unitTesting ? 0 : kLoadCompleteHideProgressBarDelay;
    [self performSelector:@selector(hideProgressBar)
               withObject:nil
               afterDelay:delay];
  }
}

- (void)hideProgressBar {
  // The UI may have been torn down while this selector was queued.  If
  // |self.delegate| is nil, it is not safe to continue.
  if (!self.delegate)
    return;

  [_determinateProgressView setHidden:YES];
  _prerenderAnimating = NO;
}

- (void)showReloadButton {
  if (!IsCompactTablet(self.view)) {
    [_reloadButton setHidden:NO];
    [_stopButton setHidden:YES];
  }
}

- (void)showStopButton {
  if (!IsCompactTablet(self.view)) {
    [_reloadButton setHidden:YES];
    [_stopButton setHidden:NO];
  }
}

- (void)updateToolbarAlphaForFrame:(CGRect)frame {
  // Don't update the toolbar buttons if we are animating for a transition.
  if (self.view.animatingTransition || [self isOmniboxFirstResponder])
    return;

  CGFloat distanceOffscreen =
      IsIPadIdiom()
          ? fmax((kIPadToolbarY - frame.origin.y) - kScrollFadeDistance, 0)
          : -1 * frame.origin.y;
  CGFloat fraction = 1 - fmin(distanceOffscreen / kScrollFadeDistance, 1);
  if (![_locationBarView.textField isFirstResponder])
    [self setStandardControlsAlpha:fraction];

  [_backButton setAlpha:fraction];
  if ([_forwardButton isEnabled])
    [_forwardButton setAlpha:fraction];
  [_reloadButton setAlpha:fraction];
  [_omniboxBackground setAlpha:fraction];
  [_locationBarView setAlpha:fraction];
  [_starButton setAlpha:fraction];
  [_voiceSearchButton setAlpha:fraction];
}

- (void)updateToolbarButtons {
  [super updateStandardButtons];
  _lastKnownTraitCollection = [UITraitCollection
      traitCollectionWithTraitsFromCollections:@[ self.view.traitCollection ]];
  if (IsIPadIdiom()) {
    // Update toolbar accessory views.
    BOOL isCompactTabletView = IsCompactTablet(self.view);
    [_voiceSearchButton setHidden:isCompactTabletView];
    [_starButton setHidden:isCompactTabletView];
    [_reloadButton setHidden:isCompactTabletView];
    [_stopButton setHidden:isCompactTabletView];
    [self updateToolbarState];

    if ([_locationBarView.textField isFirstResponder]) {
      [_locationBarView.textField reloadInputViews];
    }

    // Re-layout toolbar and omnibox.
    [_webToolbar setFrame:[self specificControlsArea]];
    [self layoutOmnibox];
  }
}

#pragma mark TTS.

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

- (void)audioReadyForPlayback:(NSNotification*)notification {
  if (![_voiceSearchButton isHidden]) {
    // Only trigger TTS playback when the voice search button is visible.
    TextToSpeechPlayer* TTSPlayer =
        base::mac::ObjCCastStrict<TextToSpeechPlayer>(notification.object);
    [TTSPlayer beginPlayback];
  }
}

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

#pragma mark Omnibox Animation.

- (void)animateMaterialOmnibox {
  // The iPad omnibox does not animate.
  if (IsIPadIdiom())
    return [self layoutOmnibox];

  CGRect newOmniboxFrame = [self newOmniboxFrame];
  BOOL growOmnibox = [_locationBarView.textField isFirstResponder];

  // Determine the starting and ending bounds and position for
  // |_locationBarView|. Increasing the height of _locationBarView results in
  // the text inside it jumping vertically during the animation, so the height
  // change will not be animated.
  CGRect fromBounds = [_locationBarView layer].bounds;
  LayoutRect toLayout =
      LayoutRectForRectInBoundingRect(newOmniboxFrame, [_webToolbar bounds]);
  CGRect toBounds = CGRectZero;
  CGPoint fromPosition = [_locationBarView layer].position;
  CGPoint toPosition = fromPosition;
  CGRect omniboxRect = LayoutRectGetRect(kOmniboxFrame[IPHONE_IDIOM]);
  if (growOmnibox) {
    toLayout.size = CGSizeMake([_webToolbar bounds].size.width -
                                   self.cancelButton.frame.size.width -
                                   kCancelButtonLeadingMargin,
                               omniboxRect.size.height);
    toLayout.position.leading = 0;
    if (_incognito) {
      // Adjust the width and leading of the omnibox to account for the
      // incognito icon.
      // TODO(crbug/525943): Refactor so this value isn't calculated here, and
      // instead is calculated in -newOmniboxFrame?
      // (include in (crbug/525943) refactor).
      LayoutRect incognitioIconLayout = LayoutRectForRectInBoundingRect(
          [_incognitoIcon frame], [_webToolbar frame]);
      CGFloat trailingEdge = LayoutRectGetTrailingEdge(incognitioIconLayout);
      toLayout.size.width -= trailingEdge;
      toLayout.position.leading = trailingEdge;
    }
  } else {
    // Shrink the omnibox to its original width.
    toLayout.size =
        CGSizeMake(newOmniboxFrame.size.width, omniboxRect.size.height);
  }
  toBounds = LayoutRectGetBoundsRect(toLayout);
  toPosition.x = LayoutRectGetPositionForAnchor(
                     toLayout, [_locationBarView layer].anchorPoint)
                     .x;

  // Determine starting and ending bounds for |_omniboxBackground|.
  // _omniboxBackground is needed to simulate the omnibox growing vertically and
  // covering the entire height of the toolbar.
  CGRect backgroundFromBounds = [_omniboxBackground layer].bounds;
  CGRect backgroundToBounds = CGRectZero;
  CGPoint backgroundFromPosition = [_omniboxBackground layer].position;
  CGPoint backgroundToPosition = CGPointZero;
  if (growOmnibox) {
    // Grow the background to cover the whole toolbar.
    backgroundToBounds = [_clippingView bounds];
    backgroundToPosition.x = backgroundToBounds.size.width / 2;
    backgroundToPosition.y = backgroundToBounds.size.height / 2;
    // Increase the bounds of the background so that the border extends past the
    // toolbar and is clipped.
    backgroundToBounds = CGRectInset(backgroundToBounds, -2, -2);
  } else {
    // Shrink the background to the same bounds as the omnibox.
    backgroundToBounds = toBounds;
    backgroundToPosition = toPosition;
    backgroundToPosition.y += StatusBarHeight();
  }

  // Is the omnibox already at the new size? Then there's nothing to animate.
  if (CGRectEqualToRect([_locationBarView layer].bounds, toBounds))
    return;

  [self animateStandardControlsForOmniboxExpansion:growOmnibox];
  if (growOmnibox) {
    [self fadeOutNavigationControls];
    [self fadeInOmniboxTrailingView];

    if (_locationBar.get()->IsShowingPlaceholderWhileCollapsed())
      [self fadeOutOmniboxLeadingView];
    else
      [_locationBarView leadingButton].alpha = 0;

    if (_incognito)
      [self fadeInIncognitoIcon];
  } else {
    [self fadeInNavigationControls];
    [self fadeOutOmniboxTrailingView];

    if (_locationBar.get()->IsShowingPlaceholderWhileCollapsed())
      [self fadeInOmniboxLeadingView];
    else
      [_locationBarView leadingButton].alpha = 1;

    if (_incognito)
      [self fadeOutIncognitoIcon];
  }

  // Animate the new bounds and position for the omnibox and background.
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    // Re-layout the omnibox's subviews after the animation to allow VoiceOver
    // to select the clear text button.
    [_locationBarView setNeedsLayout];
  }];
  CGFloat duration = ios::material::kDuration1;
  // If app is on the regular New Tab Page, make this animation occur instantly
  // since this page has a fakebox to omnibox transition.
  web::WebState* webState = [self.delegate currentWebState];
  if (webState && webState->GetVisibleURL() == GURL(kChromeUINewTabURL) &&
      !_incognito) {
    duration = 0.0;
  }
  [CATransaction setAnimationDuration:duration];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseInOut)];

  // TODO(crbug.com/525943): As part of animation cleanup, refactor
  // these animations into groups produced by FrameAnimationMake() from
  // animation_util, and do all of the bounds/position calculations above in
  // terms of frames.

  _locationBarView.frame = LayoutRectGetRect(toLayout);
  CABasicAnimation* resizeAnimation =
      [CABasicAnimation animationWithKeyPath:@"bounds"];
  resizeAnimation.delegate = self;
  [resizeAnimation setValue:@"resizeOmnibox" forKey:@"id"];
  resizeAnimation.fromValue = [NSValue valueWithCGRect:fromBounds];
  resizeAnimation.toValue = [NSValue valueWithCGRect:toBounds];
  [_locationBarView layer].bounds = toBounds;
  [[_locationBarView layer] addAnimation:resizeAnimation
                                  forKey:@"resizeBounds"];
  CABasicAnimation* positionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  positionAnimation.fromValue = [NSValue valueWithCGPoint:fromPosition];
  positionAnimation.toValue = [NSValue valueWithCGPoint:toPosition];
  [_locationBarView layer].position = toPosition;
  [[_locationBarView layer] addAnimation:positionAnimation
                                  forKey:@"movePosition"];

  resizeAnimation = [CABasicAnimation animationWithKeyPath:@"bounds"];
  resizeAnimation.fromValue = [NSValue valueWithCGRect:backgroundFromBounds];
  resizeAnimation.toValue = [NSValue valueWithCGRect:backgroundToBounds];
  [_omniboxBackground layer].bounds = backgroundToBounds;
  [[_omniboxBackground layer] addAnimation:resizeAnimation
                                    forKey:@"resizeBounds"];
  CABasicAnimation* backgroundPositionAnimation =
      [CABasicAnimation animationWithKeyPath:@"position"];
  backgroundPositionAnimation.fromValue =
      [NSValue valueWithCGPoint:backgroundFromPosition];
  backgroundPositionAnimation.toValue =
      [NSValue valueWithCGPoint:backgroundToPosition];
  [_omniboxBackground layer].position = backgroundToPosition;
  [[_omniboxBackground layer] addAnimation:backgroundPositionAnimation
                                    forKey:@"movePosition"];
  [CATransaction commit];
}

- (void)fadeInOmniboxTrailingView {
  UIView* trailingView = [_locationBarView.textField rightView];
  trailingView.alpha = 0;
  [_cancelButton setAlpha:0];
  [_cancelButton setHidden:NO];

  // Instead of passing a delay into -fadeInView:, wait to call -fadeInView:.
  // The CABasicAnimation's start and end positions are calculated immediately
  // instead of after the animation's delay, but the omnibox's layer isn't set
  // yet to its final state and as a result the start and end positions will not
  // be correct.
  dispatch_time_t delay = dispatch_time(
      DISPATCH_TIME_NOW, ios::material::kDuration6 * NSEC_PER_SEC);
  dispatch_after(delay, dispatch_get_main_queue(), ^(void) {
    [self fadeInView:trailingView
        fromLeadingOffset:kPositionAnimationLeadingOffset
             withDuration:ios::material::kDuration1
               afterDelay:0.2];
    [self fadeInView:_cancelButton
        fromLeadingOffset:kPositionAnimationLeadingOffset
             withDuration:ios::material::kDuration1
               afterDelay:0.1];
  });
}

- (void)fadeInOmniboxLeadingView {
  [_locationBarView fadeInLeadingButton];
}

- (void)fadeOutOmniboxTrailingView {
  UIView* trailingView = [_locationBarView.textField rightView];

  // Animate the opacity of the trailingView to 0.
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration4];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  [CATransaction setCompletionBlock:^{
    [_cancelButton setHidden:YES];
  }];
  CABasicAnimation* fadeOut =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  fadeOut.fromValue = @1;
  fadeOut.toValue = @0;
  trailingView.layer.opacity = 0;
  [trailingView.layer addAnimation:fadeOut forKey:@"fade"];

  // Animate the trailingView |kPositionAnimationLeadingOffset| pixels towards
  // the
  // leading edge.
  CABasicAnimation* shift = [CABasicAnimation animationWithKeyPath:@"position"];
  CGPoint startPosition = [trailingView layer].position;
  CGPoint endPosition =
      CGPointLayoutOffset(startPosition, kPositionAnimationLeadingOffset);
  shift.fromValue = [NSValue valueWithCGPoint:startPosition];
  shift.toValue = [NSValue valueWithCGPoint:endPosition];
  [[trailingView layer] addAnimation:shift forKey:@"shift"];

  [_cancelButton layer].opacity = 0;
  [[_cancelButton layer] addAnimation:fadeOut forKey:@"fade"];

  CABasicAnimation* shiftCancelButton =
      [CABasicAnimation animationWithKeyPath:@"position"];
  startPosition = [_cancelButton layer].position;
  endPosition =
      CGPointLayoutOffset(startPosition, kPositionAnimationLeadingOffset);
  shiftCancelButton.fromValue = [NSValue valueWithCGPoint:startPosition];
  shiftCancelButton.toValue = [NSValue valueWithCGPoint:endPosition];
  [[_cancelButton layer] addAnimation:shiftCancelButton forKey:@"shift"];

  [CATransaction commit];
}

- (void)fadeOutOmniboxLeadingView {
  [_locationBarView fadeOutLeadingButton];
}

#pragma mark Toolbar Buttons Animation.

- (void)fadeInIncognitoIcon {
  DCHECK(_incognito);
  DCHECK(!IsIPadIdiom());
  [self fadeInView:_incognitoIcon
      fromLeadingOffset:-kPositionAnimationLeadingOffset
           withDuration:ios::material::kDuration2
             afterDelay:ios::material::kDuration2];
}

- (void)fadeOutIncognitoIcon {
  DCHECK(_incognito);
  DCHECK(!IsIPadIdiom());

  // Animate the opacity of the incognito icon to 0.
  CALayer* layer = [_incognitoIcon layer];
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration4];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  CABasicAnimation* fadeOut =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  fadeOut.fromValue = @1;
  fadeOut.toValue = @0;
  layer.opacity = 0;
  [layer addAnimation:fadeOut forKey:@"fade"];

  // Animate the incognito icon |kPositionAnimationLeadingOffset| pixels
  // trailing.
  CABasicAnimation* shift = [CABasicAnimation animationWithKeyPath:@"position"];
  CGPoint startPosition = layer.position;
  // Constant is a leading value, so invert it to move in the trailing
  // direction.
  CGPoint endPosition =
      CGPointLayoutOffset(startPosition, -kPositionAnimationLeadingOffset);
  shift.fromValue = [NSValue valueWithCGPoint:startPosition];
  shift.toValue = [NSValue valueWithCGPoint:endPosition];
  [layer addAnimation:shift forKey:@"shift"];
  [CATransaction commit];
}

- (void)fadeInNavigationControls {
  // Determine which navigation buttons are visible and need to be animated.
  UIView* firstView = nil;
  UIView* secondView = nil;
  if ([_forwardButton isEnabled]) {
    firstView = _forwardButton;
    secondView = _backButton;
  } else {
    firstView = _backButton;
  }

  [self fadeInView:firstView
      fromLeadingOffset:kPositionAnimationLeadingOffset
           withDuration:ios::material::kDuration2
             afterDelay:ios::material::kDuration1];

  if (secondView) {
    [self fadeInView:secondView
        fromLeadingOffset:kPositionAnimationLeadingOffset
             withDuration:ios::material::kDuration2
               afterDelay:ios::material::kDuration3];
  }
}

- (void)fadeOutNavigationControls {
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration2];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  CABasicAnimation* fadeButtons =
      [CABasicAnimation animationWithKeyPath:@"opacity"];
  fadeButtons.fromValue = @1;
  fadeButtons.toValue = @0;
  if ([_forwardButton isEnabled]) {
    [_forwardButton layer].opacity = 0;
    [[_forwardButton layer] addAnimation:fadeButtons forKey:@"fadeOut"];
  }
  [_backButton layer].opacity = 0;
  [[_backButton layer] addAnimation:fadeButtons forKey:@"fadeOut"];
  [CATransaction commit];

  // Animate the navigation buttons 10 pixels to the left.
  [CATransaction begin];
  [CATransaction setAnimationDuration:ios::material::kDuration1];
  [CATransaction
      setAnimationTimingFunction:TimingFunction(ios::material::CurveEaseIn)];
  CABasicAnimation* shiftBackButton =
      [CABasicAnimation animationWithKeyPath:@"position"];
  CGPoint startPosition = [_backButton layer].position;
  CGPoint endPosition =
      CGPointLayoutOffset(startPosition, kPositionAnimationLeadingOffset);
  shiftBackButton.fromValue = [NSValue valueWithCGPoint:startPosition];
  shiftBackButton.toValue = [NSValue valueWithCGPoint:endPosition];
  [[_backButton layer] addAnimation:shiftBackButton forKey:@"shiftButton"];
  CABasicAnimation* shiftForwardButton =
      [CABasicAnimation animationWithKeyPath:@"position"];
  startPosition = [_forwardButton layer].position;
  endPosition =
      CGPointLayoutOffset(startPosition, kPositionAnimationLeadingOffset);
  shiftForwardButton.fromValue = [NSValue valueWithCGPoint:startPosition];
  shiftForwardButton.toValue = [NSValue valueWithCGPoint:endPosition];
  [[_forwardButton layer] addAnimation:shiftForwardButton
                                forKey:@"shiftButton"];
  [CATransaction commit];
}

#pragma mark Omnibox Cancel Button.

- (UIButton*)cancelButton {
  if (_cancelButton)
    return _cancelButton;
  _cancelButton = [UIButton buttonWithType:UIButtonTypeCustom];
  NSString* collapseName = _incognito ? @"collapse_incognito" : @"collapse";
  [_cancelButton setImage:[UIImage imageNamed:collapseName]
                 forState:UIControlStateNormal];
  NSString* collapsePressedName =
      _incognito ? @"collapse_pressed_incognito" : @"collapse_pressed";
  [_cancelButton setImage:[UIImage imageNamed:collapsePressedName]
                 forState:UIControlStateHighlighted];
  [_cancelButton setAccessibilityLabel:l10n_util::GetNSString(IDS_CANCEL)];
  [_cancelButton setAutoresizingMask:UIViewAutoresizingFlexibleLeadingMargin() |
                                     UIViewAutoresizingFlexibleHeight];
  [_cancelButton addTarget:self
                    action:@selector(cancelButtonPressed:)
          forControlEvents:UIControlEventTouchUpInside];

  [_webToolbar addSubview:_cancelButton];
  return _cancelButton;
}

- (void)cancelButtonPressed:(id)sender {
  [self cancelOmniboxEdit];
}

- (void)layoutCancelButton {
  CGFloat height = CGRectGetHeight([self specificControlsArea]) -
                   kCancelButtonTopMargin - kCancelButtonBottomMargin;
  // Using -specificControlsArea can sometimes be wrong in
  // an autolayout world, such as if the view is rotated with the app
  // backgrounded. It's safer to use -SafeAreaInsetsForView.
  if (IsSafeAreaCompatibleToolbarEnabled()) {
    UIEdgeInsets safeAreaInsets = SafeAreaInsetsForView(self.contentView);
    height = CGRectGetHeight(UIEdgeInsetsInsetRect(self.contentView.bounds,
                                                   safeAreaInsets)) -
             kCancelButtonTopMargin - kCancelButtonBottomMargin;
  }
  LayoutRect cancelButtonLayout;
  cancelButtonLayout.position.leading =
      CGRectGetWidth([_webToolbar bounds]) - height;
  cancelButtonLayout.boundingWidth = CGRectGetWidth([_webToolbar bounds]);
  cancelButtonLayout.position.originY = kCancelButtonTopMargin;
  cancelButtonLayout.size = CGSizeMake(kCancelButtonWidth, height);

  CGRect frame = LayoutRectGetRect(cancelButtonLayout);
  // Use the property to force creation.
  [self.cancelButton setFrame:frame];
}

#pragma mark Helpers

- (void)toolbarVoiceSearchButtonPressed:(id)sender {
  if (ios::GetChromeBrowserProvider()
          ->GetVoiceSearchProvider()
          ->IsVoiceSearchEnabled()) {
    UIView* view = base::mac::ObjCCastStrict<UIView>(sender);
    StartVoiceSearchCommand* command =
        [[StartVoiceSearchCommand alloc] initWithOriginView:view];
    [self.dispatcher startVoiceSearch:command];
  }
}

- (void)handleLongPress:(UILongPressGestureRecognizer*)gesture {
  if (gesture.state != UIGestureRecognizerStateBegan)
    return;

  if (gesture.view == _backButton) {
    [self.dispatcher showTabHistoryPopupForBackwardHistory];
  } else if (gesture.view == _forwardButton) {
    [self.dispatcher showTabHistoryPopupForForwardHistory];
  }
}

- (void)loadURLForQuery:(NSString*)query {
  GURL searchURL;
  metrics::OmniboxInputType type = AutocompleteInput::Parse(
      base::SysNSStringToUTF16(query), std::string(),
      AutocompleteSchemeClassifierImpl(), nullptr, nullptr, &searchURL);
  if (type != metrics::OmniboxInputType::URL || !searchURL.is_valid()) {
    searchURL = GetDefaultSearchURLForSearchTerms(
        ios::TemplateURLServiceFactory::GetForBrowserState(_browserState),
        base::SysNSStringToUTF16(query));
  }
  if (searchURL.is_valid()) {
    // It is necessary to include PAGE_TRANSITION_FROM_ADDRESS_BAR in the
    // transition type is so that query-in-the-omnibox is triggered for the
    // URL.
    ui::PageTransition transition = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_LINK | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    [self.urlLoader loadURL:GURL(searchURL)
                   referrer:web::Referrer()
                 transition:transition
          rendererInitiated:NO];
  }
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(ios::GetChromeBrowserProvider()
             ->GetVoiceSearchProvider()
             ->IsVoiceSearchEnabled());
  [self.dispatcher preloadVoiceSearch];
}

#pragma mark - UIViewController

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  if (!IsIPadIdiom()) {
    if (IsSafeAreaCompatibleToolbarEnabled()) {
      // The clipping view's height is supposed to match the toolbar's height.
      // The clipping view can't match the toolbar's height with autoresizing
      // masks because the clipping view is not a direct child of the toolbar.
      // Therefore we manually update the clipping view's height whenever the
      // toolbar's height changes, which as of M63 can only occur if the
      // safe area insets change.
      [self layoutClippingView];
    }
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // The popup positions itself as a static frame below the web toolbar.  This
  // will no longer be necessary post omnibox popup boxing.
  if (_popupView) {
    _popupView->UpdatePopupAppearance();
  }
}

@end

#pragma mark - Testing only.

@implementation WebToolbarController (Testing)

- (BOOL)isForwardButtonEnabled {
  return [_forwardButton isEnabled];
}

- (BOOL)isBackButtonEnabled {
  return [_backButton isEnabled];
}

- (BOOL)isStarButtonSelected {
  return [_starButton isSelected];
}

- (void)setUnitTesting:(BOOL)unitTesting {
  _unitTesting = unitTesting;
}

- (CGFloat)loadProgressFraction {
  return [_determinateProgressView progress];
}

- (std::string)getLocationText {
  return base::UTF16ToUTF8([_locationBarView.textField displayedText]);
}

- (BOOL)isLoading {
  return ![_determinateProgressView isHidden];
}

- (BOOL)isPrerenderAnimationRunning {
  return _prerenderAnimating;
}

- (OmniboxTextFieldIOS*)omnibox {
  return _locationBarView.textField;
}

@end
