// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/incognito_panel_controller.h"

#include <string>

#import "base/mac/scoped_nsobject.h"
#include "components/google/core/browser/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ios/web/public/referrer.h"
#import "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
// The URL for the the Learn More page shown on incognito new tab.
// Taken from ntp_resource_cache.cc.
const char kLearnMoreIncognitoUrl[] =
    "https://www.google.com/support/chrome/bin/answer.py?answer=95464";

GURL GetUrlWithLang(const GURL& url) {
  std::string locale = GetApplicationContext()->GetApplicationLocale();
  return google_util::AppendGoogleLocaleParam(url, locale);
}

const CGFloat kDistanceToFadeToolbar = 50.0;
const int kLinkColor = 0x03A9F4;
}

// The scrollview containing the views. Its content's size is constrained on its
// superview's size.
@interface IncognitoNTPView : UIScrollView

// Returns an autoreleased label with the right format.
- (UILabel*)labelWithString:(NSString*)string
                       font:(UIFont*)font
                      alpha:(float)alpha;

// Triggers a navigation to the help page.
- (void)learnMoreButtonPressed;

@end

@implementation IncognitoNTPView {
  base::WeakNSProtocol<id<UrlLoader>> _loader;
  base::scoped_nsobject<UIView> _containerView;

  // Constraint ensuring that |containerView| is at least as high as the
  // superview of the IncognitoNTPView, i.e. the Incognito panel.
  // This ensures that if the Incognito panel is higher than a compact
  // |containerView|, the |containerView|'s |topGuide| and |bottomGuide| are
  // forced to expand, centering the views in between them.
  base::scoped_nsobject<NSLayoutConstraint> _containerVerticalConstraint;

  // Constraint ensuring that |containerView| is as wide as the superview of the
  // the IncognitoNTPView, i.e. the Incognito panel.
  base::scoped_nsobject<NSLayoutConstraint> _containerHorizontalConstraint;
}

- (instancetype)initWithFrame:(CGRect)frame urlLoader:(id<UrlLoader>)loader {
  self = [super initWithFrame:frame];
  if (self) {
    _loader.reset(loader);

    self.alwaysBounceVertical = YES;

    // Container in which all the subviews (image, labels, button) are added.
    _containerView.reset([[UIView alloc] initWithFrame:frame]);
    [_containerView setTranslatesAutoresizingMaskIntoConstraints:NO];

    // Incognito image.
    base::scoped_nsobject<UIImageView> incognitoImage([[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"incognito_icon"]]);
    [incognitoImage setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_containerView addSubview:incognitoImage];

    // Title.
    UIFont* titleFont =
        [[MDFRobotoFontLoader sharedInstance] lightFontOfSize:24];
    base::scoped_nsobject<UILabel> incognitoTabHeading(
        [[self labelWithString:l10n_util::GetNSString(IDS_NEW_TAB_OTR_HEADING)
                          font:titleFont
                         alpha:0.8] retain]);
    [_containerView addSubview:incognitoTabHeading];

    // Description paragraph.
    UIFont* regularFont =
        [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14];
    base::scoped_nsobject<UILabel> incognitoTabDescription([[self
        labelWithString:l10n_util::GetNSString(IDS_NEW_TAB_OTR_DESCRIPTION)
                   font:regularFont
                  alpha:0.7] retain]);
    [_containerView addSubview:incognitoTabDescription];

    // Warning paragraph.
    base::scoped_nsobject<UILabel> incognitoTabWarning([[self
        labelWithString:l10n_util::GetNSString(IDS_NEW_TAB_OTR_MESSAGE_WARNING)
                   font:regularFont
                  alpha:0.7] retain]);
    [_containerView addSubview:incognitoTabWarning];

    // Learn more button.
    base::scoped_nsobject<MDCButton> learnMore([[MDCFlatButton alloc] init]);
    UIColor* inkColor =
        [[[MDCPalette greyPalette] tint300] colorWithAlphaComponent:0.25];
    [learnMore setInkColor:inkColor];
    [learnMore setTranslatesAutoresizingMaskIntoConstraints:NO];
    [learnMore setTitle:l10n_util::GetNSString(IDS_NEW_TAB_OTR_LEARN_MORE_LINK)
               forState:UIControlStateNormal];
    [learnMore setTitleColor:UIColorFromRGB(kLinkColor)
                    forState:UIControlStateNormal];
    UIFont* buttonFont =
        [[MDFRobotoFontLoader sharedInstance] boldFontOfSize:14];
    [[learnMore titleLabel] setFont:buttonFont];
    [learnMore addTarget:self
                  action:@selector(learnMoreButtonPressed)
        forControlEvents:UIControlEventTouchUpInside];
    [_containerView addSubview:learnMore];

    // |topGuide| and |bottomGuide| exist to vertically center the sibling views
    // located in between them.
    base::scoped_nsobject<UILayoutGuide> topGuide([[UILayoutGuide alloc] init]);
    base::scoped_nsobject<UILayoutGuide> bottomGuide(
        [[UILayoutGuide alloc] init]);
    [_containerView addLayoutGuide:topGuide];
    [_containerView addLayoutGuide:bottomGuide];

    NSDictionary* viewsDictionary = @{
      @"topGuide" : topGuide.get(),
      @"image" : incognitoImage.get(),
      @"heading" : incognitoTabHeading.get(),
      @"description" : incognitoTabDescription.get(),
      @"warning" : incognitoTabWarning.get(),
      @"learnMoreButton" : learnMore.get(),
      @"bottomGuide" : bottomGuide.get(),
    };
    NSArray* constraints = @[
      @"V:|-0-[topGuide(>=12)]-[image]-24-[heading]-32-[description]",
      @"V:[description]-32-[warning]-32-[learnMoreButton]",
      @"V:[learnMoreButton]-[bottomGuide]-0-|",
      @"H:|-(>=24)-[heading]-(>=24)-|",
      @"H:|-(>=24)-[description(==416@999)]-(>=24)-|",
      @"H:|-(>=24)-[warning(==416@999)]-(>=24)-|"
    ];
    ApplyVisualConstraintsWithOptions(constraints, viewsDictionary,
                                      LayoutOptionForRTLSupport(),
                                      _containerView);

    AddSameCenterXConstraint(_containerView, incognitoImage);
    AddSameCenterXConstraint(_containerView, incognitoTabHeading);
    AddSameCenterXConstraint(_containerView, incognitoTabDescription);
    AddSameCenterXConstraint(_containerView, incognitoTabWarning);
    AddSameCenterXConstraint(_containerView, learnMore);

    // The bottom guide is twice as big as the top guide.
    [_containerView addConstraint:[NSLayoutConstraint
                                      constraintWithItem:bottomGuide
                                               attribute:NSLayoutAttributeHeight
                                               relatedBy:NSLayoutRelationEqual
                                                  toItem:topGuide
                                               attribute:NSLayoutAttributeHeight
                                              multiplier:2
                                                constant:0]];

    [self addSubview:_containerView];

    // Constraints comunicating the size of the contentView to the scrollview.
    // See UIScrollView autolayout information at
    // https://developer.apple.com/library/ios/releasenotes/General/RN-iOSSDK-6_0/index.html
    viewsDictionary = @{ @"containerView" : _containerView.get() };
    constraints = @[
      @"V:|-0-[containerView]-0-|",
      @"H:|-0-[containerView]-0-|",
    ];
    ApplyVisualConstraintsWithOptions(constraints, viewsDictionary,
                                      LayoutOptionForRTLSupport(), self);
  }
  return self;
}

- (UILabel*)labelWithString:(NSString*)string
                       font:(UIFont*)font
                      alpha:(float)alpha {
  base::scoped_nsobject<NSMutableAttributedString> attributedString(
      [[NSMutableAttributedString alloc] initWithString:string]);
  base::scoped_nsobject<NSMutableParagraphStyle> paragraphStyle(
      [[NSMutableParagraphStyle alloc] init]);
  [paragraphStyle setLineSpacing:4];
  [paragraphStyle setAlignment:NSTextAlignmentJustified];
  [attributedString addAttribute:NSParagraphStyleAttributeName
                           value:paragraphStyle
                           range:NSMakeRange(0, string.length)];
  base::scoped_nsobject<UILabel> label(
      [[UILabel alloc] initWithFrame:CGRectZero]);
  [label setTranslatesAutoresizingMaskIntoConstraints:NO];
  [label setNumberOfLines:0];
  [label setFont:font];
  [label setAttributedText:attributedString];
  [label setTextColor:[UIColor colorWithWhite:1.0 alpha:alpha]];
  return label.autorelease();
}

- (void)learnMoreButtonPressed {
  GURL gurl = GetUrlWithLang(GURL(kLearnMoreIncognitoUrl));
  [_loader loadURL:gurl
               referrer:web::Referrer()
             transition:ui::PAGE_TRANSITION_LINK
      rendererInitiated:NO];
}

#pragma mark - UIView overrides

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  _containerHorizontalConstraint.reset(
      [[NSLayoutConstraint constraintWithItem:_containerView.get()
                                    attribute:NSLayoutAttributeWidth
                                    relatedBy:NSLayoutRelationEqual
                                       toItem:[self superview]
                                    attribute:NSLayoutAttributeWidth
                                   multiplier:1
                                     constant:0] retain]);
  _containerVerticalConstraint.reset(
      [[NSLayoutConstraint constraintWithItem:_containerView.get()
                                    attribute:NSLayoutAttributeHeight
                                    relatedBy:NSLayoutRelationGreaterThanOrEqual
                                       toItem:[self superview]
                                    attribute:NSLayoutAttributeHeight
                                   multiplier:1
                                     constant:0] retain]);
  [[self superview] addConstraint:_containerHorizontalConstraint.get()];
  [[self superview] addConstraint:_containerVerticalConstraint.get()];
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [[self superview] removeConstraint:_containerHorizontalConstraint.get()];
  [[self superview] removeConstraint:_containerVerticalConstraint.get()];
  _containerHorizontalConstraint.reset();
  _containerVerticalConstraint.reset();
  [super willMoveToSuperview:newSuperview];
}

@end

@interface IncognitoPanelController ()<UIScrollViewDelegate>
// Calculate the background alpha for the toolbar based on how much |scrollView|
// has scrolled up.
- (CGFloat)toolbarAlphaForScrollView:(UIScrollView*)scrollView;
@end

@implementation IncognitoPanelController {
  // Delegate for updating the toolbar's background alpha.
  base::WeakNSProtocol<id<WebToolbarDelegate>> _webToolbarDelegate;

  // The view containing the scrollview.
  // The purpose of this view is to be used to set the size
  // of the contentView of the scrollview with constraints.
  base::scoped_nsobject<UIView> _view;

  // The scrollview containing the actual views.
  base::scoped_nsobject<IncognitoNTPView> _incognitoView;
}

// Property declared in NewTabPagePanelProtocol.
@synthesize delegate = _delegate;

- (id)initWithLoader:(id<UrlLoader>)loader
          browserState:(ios::ChromeBrowserState*)browserState
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate {
  self = [super init];
  if (self) {
    _view.reset([[UIView alloc]
        initWithFrame:[UIApplication sharedApplication].keyWindow.bounds]);
    [_view setAccessibilityIdentifier:@"NTP Incognito Panel"];
    [_view setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                               UIViewAutoresizingFlexibleWidth];
    _incognitoView.reset([[IncognitoNTPView alloc]
        initWithFrame:[UIApplication sharedApplication].keyWindow.bounds
            urlLoader:loader]);
    [_incognitoView setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                        UIViewAutoresizingFlexibleWidth];

    if (IsIPadIdiom()) {
      [_incognitoView setBackgroundColor:[UIColor clearColor]];
    } else {
      [_incognitoView
          setBackgroundColor:[UIColor colorWithWhite:34 / 255.0 alpha:1.0]];
    }
    if (!IsIPadIdiom()) {
      [_incognitoView setDelegate:self];
      _webToolbarDelegate.reset(webToolbarDelegate);
      [_webToolbarDelegate updateToolbarBackgroundAlpha:0];
    }
    [_view addSubview:_incognitoView];
  }
  return self;
}

- (CGFloat)toolbarAlphaForScrollView:(UIScrollView*)scrollView {
  CGFloat alpha = scrollView.contentOffset.y / kDistanceToFadeToolbar;
  return MAX(MIN(alpha, 1), 0);
}

- (void)dealloc {
  [_webToolbarDelegate updateToolbarBackgroundAlpha:1];
  [_incognitoView setDelegate:nil];
  [super dealloc];
}

#pragma mark -
#pragma mark NewTabPagePanelProtocol

- (void)reload {
}

- (void)wasShown {
  CGFloat alpha = [self toolbarAlphaForScrollView:_incognitoView];
  [_webToolbarDelegate updateToolbarBackgroundAlpha:alpha];
}

- (void)wasHidden {
  [_webToolbarDelegate updateToolbarBackgroundAlpha:1];
}

- (void)dismissModals {
}

- (void)dismissKeyboard {
}

- (void)setScrollsToTop:(BOOL)enable {
}

- (CGFloat)alphaForBottomShadow {
  return 0;
}

- (UIView*)view {
  return _view.get();
}

#pragma mark -
#pragma mark UIScrollViewDelegate methods

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  CGFloat alpha = [self toolbarAlphaForScrollView:_incognitoView];
  [_webToolbarDelegate updateToolbarBackgroundAlpha:alpha];
}

@end
