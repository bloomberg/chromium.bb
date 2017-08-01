// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"

#import <QuartzCore/QuartzCore.h>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/threading/sequenced_worker_pool.h"
#import "components/image_fetcher/ios/ios_image_data_fetcher_wrapper.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_material_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_positioner.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_popup_view_suggestions_delegate.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/geometry/rect.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kExpandAnimationDuration = 0.1;
const CGFloat kCollapseAnimationDuration = 0.05;
const CGFloat kWhiteBackgroundHeight = 74;
NS_INLINE CGFloat ShadowHeight() {
  return IsIPadIdiom() ? 10 : 0;
}
}  // namespace

using base::UserMetricsAction;

OmniboxPopupViewIOS::OmniboxPopupViewIOS(
    ios::ChromeBrowserState* browser_state,
    OmniboxEditModel* edit_model,
    OmniboxPopupViewSuggestionsDelegate* delegate,
    id<OmniboxPopupPositioner> positioner)
    : model_(new OmniboxPopupModel(this, edit_model)),
      delegate_(delegate),
      positioner_(positioner),
      is_open_(false) {
  DCHECK(delegate);
  DCHECK(browser_state);
  DCHECK(edit_model);

  std::unique_ptr<image_fetcher::IOSImageDataFetcherWrapper> imageFetcher =
      base::MakeUnique<image_fetcher::IOSImageDataFetcherWrapper>(
          browser_state->GetRequestContext(),
          web::WebThread::GetBlockingPool());

  popup_controller_.reset([[OmniboxPopupMaterialViewController alloc]
      initWithPopupView:this
            withFetcher:std::move(imageFetcher)]);
  [popup_controller_ setIncognito:browser_state->IsOffTheRecord()];
  popupView_.reset([[UIView alloc] initWithFrame:CGRectZero]);
  [popupView_ setClipsToBounds:YES];
  CALayer* popupLayer = [popupView_ layer];
  // Adjust popupView_'s anchor point and height so that it animates down
  // from the top when it appears.
  popupLayer.anchorPoint = CGPointMake(0.5, 0);
  UIView* popupControllerView = [popup_controller_ view];
  CGRect popupControllerFrame = popupControllerView.frame;
  popupControllerFrame.origin = CGPointZero;
  popupControllerView.frame = popupControllerFrame;
  [popupView_ addSubview:popupControllerView];
  if (IsIPadIdiom()) {
    [popupView_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth];
    UIImageView* shadowView = [[UIImageView alloc]
        initWithImage:NativeImage(IDR_IOS_TOOLBAR_SHADOW_FULL_BLEED)];
    [shadowView setUserInteractionEnabled:NO];
    [shadowView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [popupView_ addSubview:shadowView];

    // Add constraints to position |shadowView| at the bottom of |popupView_|
    // with the same width as |popupView_|.
    NSDictionary* views = NSDictionaryOfVariableBindings(shadowView);
    [popupView_
        addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"H:|[shadowView]|"
                                               options:0
                                               metrics:nil
                                                 views:views]];
    [popupView_ addConstraint:[NSLayoutConstraint
                                  constraintWithItem:shadowView
                                           attribute:NSLayoutAttributeBottom
                                           relatedBy:NSLayoutRelationEqual
                                              toItem:popupView_
                                           attribute:NSLayoutAttributeBottom
                                          multiplier:1
                                            constant:0]];
  } else {
    // Add a white background to prevent seeing the logo scroll through the
    // omnibox.
    UIView* whiteBackground = [[UIView alloc] initWithFrame:CGRectZero];
    [popupView_ addSubview:whiteBackground];
    [whiteBackground setBackgroundColor:[UIColor whiteColor]];

    // Set constraints to |whiteBackground|.
    [whiteBackground setTranslatesAutoresizingMaskIntoConstraints:NO];
    NSDictionary* metrics = @{ @"height" : @(kWhiteBackgroundHeight) };
    NSDictionary* views = NSDictionaryOfVariableBindings(whiteBackground);
    [popupView_
        addConstraints:[NSLayoutConstraint
                           constraintsWithVisualFormat:@"H:|[whiteBackground]|"
                                               options:0
                                               metrics:nil
                                                 views:views]];
    [popupView_
        addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:
                                               @"V:[whiteBackground(==height)]"
                                                               options:0
                                                               metrics:metrics
                                                                 views:views]];
    [popupView_ addConstraint:[NSLayoutConstraint
                                  constraintWithItem:whiteBackground
                                           attribute:NSLayoutAttributeBottom
                                           relatedBy:NSLayoutRelationEqual
                                              toItem:popupView_
                                           attribute:NSLayoutAttributeTop
                                          multiplier:1
                                            constant:0]];
    // |whiteBackground| extends out of |popupView_|
    [popupView_ setClipsToBounds:NO];
  }
}

OmniboxPopupViewIOS::~OmniboxPopupViewIOS() {
  // Destroy the model, in case it tries to call back into us when destroyed.
  model_.reset();
}

// Set left image to globe or magnifying glass depending on which autocomplete
// option comes first.
void OmniboxPopupViewIOS::UpdateEditViewIcon() {
  const AutocompleteResult& result = model_->result();
  const AutocompleteMatch& match = result.match_at(0);  // 0 for first result.
  int image_id = GetIconForAutocompleteMatchType(
      match.type, /* is_starred */ false, /* is_incognito */ false);
  delegate_->OnTopmostSuggestionImageChanged(image_id);
}

void OmniboxPopupViewIOS::UpdatePopupAppearance() {
  const AutocompleteResult& result = model_->result();
  UIView* view = popupView_;

  if (!is_open_ && !result.empty()) {
    // The popup is not currently open and there are results to display. Update
    // and animate the cells
    [popup_controller_ updateMatches:result withAnimation:YES];
  } else {
    // The popup is already displayed or there are no results to display. Update
    // the cells without animating.
    [popup_controller_ updateMatches:result withAnimation:NO];
  }
  is_open_ = !result.empty();

  if (is_open_) {
    // Show |result.size| on iPad.  Since iPhone can dismiss keyboard, set
    // height to frame height.
    CGFloat height = [[popup_controller_ tableView] contentSize].height;
    UIEdgeInsets insets = [[popup_controller_ tableView] contentInset];
    // Note the calculation |insets.top * 2| is correct, it should not be
    // insets.top + insets.bottom. |insets.bottom| will be larger than
    // |insets.top| when the keyboard is visible, but |parentHeight| should stay
    // the same.
    CGFloat parentHeight = height + insets.top * 2 + ShadowHeight();
    UIView* siblingView = [positioner_ popupAnchorView];
    if (!IsIPadIdiom()) {
      [view setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                UIViewAutoresizingFlexibleHeight];
      [[siblingView superview] insertSubview:view belowSubview:siblingView];
    } else {
      [[siblingView superview] insertSubview:view aboveSubview:siblingView];
    }
    CGFloat currentHeight = view.layer.bounds.size.height;
    if (currentHeight == 0)
      AnimateDropdownExpansion(parentHeight);
    else
      [view setFrame:[positioner_ popupFrame:parentHeight]];
    UIView* popupControllerView = [popup_controller_ view];
    CGRect popupControllerFrame = popupControllerView.frame;
    popupControllerFrame.size.height = view.frame.size.height - ShadowHeight();
    popupControllerView.frame = popupControllerFrame;
    UpdateEditViewIcon();
  } else {
    AnimateDropdownCollapse();
  }

  delegate_->OnResultsChanged(result);
}

void OmniboxPopupViewIOS::AnimateDropdownExpansion(CGFloat parentHeight) {
  CGRect popupFrame = [positioner_ popupFrame:parentHeight];
  CALayer* popupLayer = [popupView_ layer];
  CGRect bounds = popupLayer.bounds;
  bounds.size.height = popupFrame.size.height;
  popupLayer.bounds = bounds;

  CGRect frame = [popupView_ frame];
  frame.size.width = popupFrame.size.width;
  frame.origin.y = popupFrame.origin.y;
  [popupView_ setFrame:frame];

  CABasicAnimation* growHeight =
      [CABasicAnimation animationWithKeyPath:@"bounds.size.height"];
  growHeight.fromValue = @0;
  growHeight.toValue = [NSNumber numberWithFloat:popupFrame.size.height];
  growHeight.duration = kExpandAnimationDuration;
  growHeight.timingFunction =
      [CAMediaTimingFunction functionWithControlPoints:0.4:0:0.2:1];
  [popupLayer addAnimation:growHeight forKey:@"growHeight"];
}

void OmniboxPopupViewIOS::AnimateDropdownCollapse() {
  CALayer* popupLayer = [popupView_ layer];
  CGRect bounds = popupLayer.bounds;
  CGFloat currentHeight = bounds.size.height;
  bounds.size.height = 0;
  popupLayer.bounds = bounds;

  UIView* retainedPopupView = popupView_;
  [CATransaction begin];
  [CATransaction setCompletionBlock:^{
    [retainedPopupView removeFromSuperview];
  }];
  CABasicAnimation* shrinkHeight =
      [CABasicAnimation animationWithKeyPath:@"bounds.size.height"];
  shrinkHeight.fromValue = [NSNumber numberWithFloat:currentHeight];
  shrinkHeight.toValue = @0;
  shrinkHeight.duration = kCollapseAnimationDuration;
  shrinkHeight.timingFunction =
      [CAMediaTimingFunction functionWithControlPoints:0.4:0:1:1];
  [popupLayer addAnimation:shrinkHeight forKey:@"shrinkHeight"];
  [CATransaction commit];
}

gfx::Rect OmniboxPopupViewIOS::GetTargetBounds() {
  return gfx::Rect();
}

// For phone, allow popup to take focus (and dismiss the keyboard) on scroll.
void OmniboxPopupViewIOS::DidScroll() {
  delegate_->OnPopupDidScroll();
}

// Puts omnibox back into focus with suggested search terms.
void OmniboxPopupViewIOS::CopyToOmnibox(const base::string16& str) {
  delegate_->OnSelectedMatchForAppending(str);
}

void OmniboxPopupViewIOS::SetTextAlignment(NSTextAlignment alignment) {
  [popup_controller_ setTextAlignment:alignment];
}

bool OmniboxPopupViewIOS::IsStarredMatch(const AutocompleteMatch& match) const {
  return model_->IsStarredMatch(match);
}

void OmniboxPopupViewIOS::DeleteMatch(const AutocompleteMatch& match) const {
  model_->autocomplete_controller()->DeleteMatch(match);
}

void OmniboxPopupViewIOS::OpenURLForRow(size_t row) {
  // Crash reports tell us that |row| is sometimes indexed past the end of
  // the results array. In those cases, just ignore the request and return
  // early. See b/5813291.
  if (row >= model_->result().size())
    return;

  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  // OpenMatch() may close the popup, which will clear the result set and, by
  // extension, |match| and its contents.  So copy the relevant match out to
  // make sure it stays alive until the call completes.
  AutocompleteMatch match = model_->result().match_at(row);
  if (match.type == AutocompleteMatchType::CLIPBOARD) {
    base::RecordAction(UserMetricsAction("MobileOmniboxClipboardToURL"));
    UMA_HISTOGRAM_LONG_TIMES_100(
        "MobileOmnibox.PressedClipboardSuggestionAge",
        ClipboardRecentContent::GetInstance()->GetClipboardContentAge());
  }
  delegate_->OnSelectedMatchForOpening(match, disposition, GURL(),
                                       base::string16(), row);
}

bool OmniboxPopupViewIOS::IsOpen() const {
  return is_open_;
}
