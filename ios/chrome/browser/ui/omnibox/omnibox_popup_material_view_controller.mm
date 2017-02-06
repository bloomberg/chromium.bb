// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_popup_material_view_controller.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "ios/chrome/browser/ui/animation_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_material_row.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_popup_view_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/truncating_attributed_label.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"
#include "ios/web/public/image_fetcher/image_data_fetcher.h"
#include "net/base/escape.h"

namespace {
const int kRowCount = 6;
const CGFloat kRowHeight = 48.0;
const CGFloat kAnswerRowHeight = 64.0;
const CGFloat kTopAndBottomPadding = 8.0;
// The color of the main text of a suggest cell.
UIColor* SuggestionTextColor() {
  return [UIColor colorWithWhite:(51 / 255.0) alpha:1.0];
}
// The color of the detail text of a suggest cell.
UIColor* SuggestionDetailTextColor() {
  return [UIColor colorWithRed:(85 / 255.0)
                         green:(149 / 255.0)
                          blue:(254 / 255.0)
                         alpha:1.0];
}
// The color of the text in the portion of a search suggestion that matches the
// omnibox input text.
UIColor* DimColor() {
  return [UIColor colorWithWhite:(161 / 255.0) alpha:1.0];
}
UIColor* SuggestionTextColorIncognito() {
  return [UIColor whiteColor];
}
UIColor* DimColorIncognito() {
  return [UIColor whiteColor];
}
UIColor* BackgroundColorTablet() {
  return [UIColor whiteColor];
}
UIColor* BackgroundColorPhone() {
  return [UIColor colorWithRed:(245 / 255.0)
                         green:(245 / 255.0)
                          blue:(246 / 255.0)
                         alpha:1.0];
}
UIColor* BackgroundColorIncognito() {
  return [UIColor colorWithRed:(50 / 255.0)
                         green:(50 / 255.0)
                          blue:(50 / 255.0)
                         alpha:1.0];
}
}  // namespace

@interface OmniboxPopupMaterialViewController () {
  // CTFontRef's are needed for drawing attributed strings but are expensive
  // to create. Since we only need four, we create them here and hold on to
  // them.
  base::ScopedCFTypeRef<CTFontRef> _smallFont;
  base::ScopedCFTypeRef<CTFontRef> _bigFont;
  base::ScopedCFTypeRef<CTFontRef> _smallBoldFont;
  base::ScopedCFTypeRef<CTFontRef> _bigBoldFont;

  // Alignment of omnibox text. Popup text should match this alignment.
  NSTextAlignment _alignment;

  OmniboxPopupViewIOS* _popupView;  // weak, owns us

  // Fetcher for Answers in Suggest images.
  std::unique_ptr<web::ImageDataFetcher> imageFetcher_;

  // The data source.
  AutocompleteResult _currentResult;

  // Array containing the OmniboxPopupMaterialRow objects displayed in the view.
  base::scoped_nsobject<NSArray> _rows;

  // The height of the keyboard. Used to determine the content inset for the
  // scroll view.
  CGFloat keyboardHeight_;
}

@end

@implementation OmniboxPopupMaterialViewController

@synthesize incognito = _incognito;

#pragma mark -
#pragma mark Initialization

- (instancetype)initWithPopupView:(OmniboxPopupViewIOS*)view
                      withFetcher:
                          (std::unique_ptr<web::ImageDataFetcher>)imageFetcher {
  if ((self = [super init])) {
    _popupView = view;
    imageFetcher_ = std::move(imageFetcher);

    if (IsIPadIdiom()) {
      // The iPad keyboard can cover some of the rows of the scroll view. The
      // scroll view's content inset may need to be updated when the keyboard is
      // displayed.
      NSNotificationCenter* defaultCenter =
          [NSNotificationCenter defaultCenter];
      [defaultCenter addObserver:self
                        selector:@selector(keyboardDidShow:)
                            name:UIKeyboardDidShowNotification
                          object:nil];
    }
  }
  return self;
}

- (void)dealloc {
  self.tableView.delegate = nil;
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (UIScrollView*)scrollView {
  return (UIScrollView*)self.tableView;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Initialize the same size as the parent view, autoresize will correct this.
  [self.view setFrame:CGRectZero];
  if (_incognito) {
    self.view.backgroundColor = BackgroundColorIncognito();
  } else {
    self.view.backgroundColor =
        IsIPadIdiom() ? BackgroundColorTablet() : BackgroundColorPhone();
  }
  [self.view setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight)];

  // Cache fonts needed for omnibox attributed string.
  UIFont* smallFont = [MDCTypography body1Font];
  UIFont* bigFont = [MDCTypography subheadFont];
  UIFont* smallBoldFont = [[MDFRobotoFontLoader sharedInstance]
      mediumFontOfSize:smallFont.pointSize];
  UIFont* bigBoldFont =
      [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:bigFont.pointSize];
  _smallFont.reset(CTFontCreateWithName((CFStringRef)smallFont.fontName,
                                        smallFont.pointSize, NULL));
  _bigFont.reset(CTFontCreateWithName((CFStringRef)bigFont.fontName,
                                      bigFont.pointSize, NULL));
  _smallBoldFont.reset(CTFontCreateWithName((CFStringRef)smallBoldFont.fontName,
                                            smallBoldFont.pointSize, NULL));
  _bigBoldFont.reset(CTFontCreateWithName((CFStringRef)bigBoldFont.fontName,
                                          bigBoldFont.pointSize, NULL));
  NSMutableArray* rowsBuilder = [[[NSMutableArray alloc] init] autorelease];
  for (int i = 0; i < kRowCount; i++) {
    OmniboxPopupMaterialRow* row = [[[OmniboxPopupMaterialRow alloc]
        initWithIncognito:_incognito] autorelease];
    row.accessibilityIdentifier =
        [NSString stringWithFormat:@"omnibox suggestion %i", i];
    row.autoresizingMask = UIViewAutoresizingFlexibleWidth;
    [rowsBuilder addObject:row];
    [row.appendButton addTarget:self
                         action:@selector(appendButtonTapped:)
               forControlEvents:UIControlEventTouchUpInside];
    [row.appendButton setTag:i];
    row.rowHeight = kRowHeight;
  }
  _rows.reset([rowsBuilder copy]);

  // Table configuration.
  self.tableView.allowsMultipleSelectionDuringEditing = NO;
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.separatorInset = UIEdgeInsetsZero;
  if ([self.tableView respondsToSelector:@selector(setLayoutMargins:)]) {
    [self.tableView setLayoutMargins:UIEdgeInsetsZero];
  }
  self.automaticallyAdjustsScrollViewInsets = NO;
  [self.tableView setContentInset:UIEdgeInsetsMake(kTopAndBottomPadding, 0,
                                                   kTopAndBottomPadding, 0)];
}

- (void)didReceiveMemoryWarning {
  [super didReceiveMemoryWarning];
  if (![self isViewLoaded]) {
    _smallFont.reset();
    _bigFont.reset();
    _smallBoldFont.reset();
    _bigBoldFont.reset();
    _rows.reset();
  }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self layoutRows];
}

#pragma mark -
#pragma mark Updating data and UI

- (void)updateRow:(OmniboxPopupMaterialRow*)row
        withMatch:(const AutocompleteMatch&)match {
  const CGFloat kTextCellLeadingPadding =
      IsIPadIdiom() ? (!IsCompactTablet() ? 192 : 100) : 16;
  const CGFloat kTextCellTopPadding = 6;
  const CGFloat kDetailCellTopPadding = 26;
  const CGFloat kTextLabelHeight = 24;
  const CGFloat kTextDetailLabelHeight = 22;
  const CGFloat kAppendButtonWidth = 40;
  const CGFloat kAnswerLabelHeight = 50;
  const CGFloat kAnswerImageWidth = 30;
  const CGFloat kAnswerImageLeftPadding = -1;
  const CGFloat kAnswerImageRightPadding = 4;
  const CGFloat kAnswerImageTopPadding = -3;
  const CGFloat kAnswerRowPadding = 4;
  const BOOL alignmentRight = _alignment == NSTextAlignmentRight;

  BOOL LTRTextInRTLLayout = _alignment == NSTextAlignmentLeft && UseRTLLayout();

  // Update the frame to reflect whether we have an answer or not.
  const BOOL answerPresent = match.answer.get() != nil;
  row.rowHeight = answerPresent ? kAnswerRowHeight : kRowHeight;

  [row.detailTruncatingLabel setTextAlignment:_alignment];
  [row.textTruncatingLabel setTextAlignment:_alignment];

  // Fetch the answer image if specified.  Currently, no answer types specify an
  // image on the first line so for now we only look at the second line.
  const BOOL answerImagePresent =
      answerPresent && match.answer->second_line().image_url().is_valid();
  if (answerImagePresent) {
    web::ImageFetchedCallback callback =
        ^(const GURL& original_url, int response_code, NSData* data) {
          if (data) {
            UIImage* image =
                [UIImage imageWithData:data scale:[UIScreen mainScreen].scale];
            if (image) {
              row.answerImageView.image = image;
            }
          }
        };
    imageFetcher_->StartDownload(match.answer->second_line().image_url(),
                                 callback);

    // Answers in suggest do not support RTL, left align only.
    CGFloat imageLeftPadding =
        kTextCellLeadingPadding + kAnswerImageLeftPadding;
    if (alignmentRight) {
      imageLeftPadding =
          row.frame.size.width - (kAnswerImageWidth + kAppendButtonWidth);
    }
    CGFloat imageTopPadding =
        kDetailCellTopPadding + kAnswerRowPadding + kAnswerImageTopPadding;
    row.answerImageView.frame =
        CGRectMake(imageLeftPadding, imageTopPadding, kAnswerImageWidth,
                   kAnswerImageWidth);
    row.answerImageView.hidden = NO;
  } else {
    row.answerImageView.hidden = YES;
  }

  // DetailTextLabel and textLabel are fading labels placed in each row. The
  // textLabel is layed out above the detailTextLabel, and vertically centered
  // if the detailTextLabel is empty.
  OmniboxPopupTruncatingLabel* detailTextLabel = row.detailTruncatingLabel;
  // The width must be positive for CGContextRef to be valid.
  CGFloat labelWidth =
      MAX(40, floorf(row.frame.size.width) - kTextCellLeadingPadding);
  CGFloat labelHeight =
      answerPresent ? kAnswerLabelHeight : kTextDetailLabelHeight;
  CGFloat answerImagePadding = kAnswerImageWidth + kAnswerImageRightPadding;
  CGFloat leadingPadding =
      (answerImagePresent && !alignmentRight ? answerImagePadding : 0) +
      kTextCellLeadingPadding;
  CGFloat topPadding =
      (answerPresent ? kAnswerRowPadding : 0) + kDetailCellTopPadding;

  LayoutRect detailTextLabelLayout =
      LayoutRectMake(leadingPadding, CGRectGetWidth(self.view.bounds),
                     topPadding, labelWidth, labelHeight);
  detailTextLabel.frame = LayoutRectGetRect(detailTextLabelLayout);

  // Details should be the URL (|match.contents|). For searches |match.contents|
  // is the default search engine name, which for mobile we suppress.
  NSString* detailText = ![self isSearchMatch:match.type]
                             ? base::SysUTF16ToNSString(match.contents)
                             : nil;
  if (answerPresent) {
    detailTextLabel.attributedText =
        [self attributedStringWithAnswerLine:match.answer->second_line()];
  } else {
    const ACMatchClassifications* classifications =
        ![self isSearchMatch:match.type] ? &match.contents_class : nil;
    detailTextLabel.attributedText =
        [self attributedStringWithString:detailText
                         classifications:classifications
                               smallFont:YES
                                   color:SuggestionDetailTextColor()
                                dimColor:DimColor()];
  }
  [detailTextLabel setNeedsDisplay];

  OmniboxPopupTruncatingLabel* textLabel = row.textTruncatingLabel;
  LayoutRect textLabelLayout =
      LayoutRectMake(kTextCellLeadingPadding, CGRectGetWidth(self.view.bounds),
                     0, labelWidth, kTextLabelHeight);
  textLabel.frame = LayoutRectGetRect(textLabelLayout);

  // Match should be search term (match.contents) for searches, otherwise
  // page title (|match.description|).
  base::string16 textString =
      [self isSearchMatch:match.type] ? match.contents : match.description;
  NSString* text = base::SysUTF16ToNSString(textString);
  const ACMatchClassifications* textClassifications =
      [self isSearchMatch:match.type] ? &match.contents_class
                                      : &match.description_class;

  // If for some reason the title is empty, copy the detailText.
  if ([text length] == 0 && [detailText length] != 0) {
    text = detailText;
  }
  // Center the textLabel if detailLabel is empty.
  if (!answerPresent && [detailText length] == 0) {
    textLabel.center = CGPointMake(textLabel.center.x, floor(kRowHeight / 2));
    textLabel.frame = AlignRectToPixel(textLabel.frame);
  } else {
    CGRect frame = textLabel.frame;
    frame.origin.y = kTextCellTopPadding;
    textLabel.frame = frame;
  }
  textLabel.autoresizingMask = UIViewAutoresizingFlexibleWidth;
  UIColor* suggestionTextColor =
      _incognito ? SuggestionTextColorIncognito() : SuggestionTextColor();
  UIColor* dimColor = _incognito ? DimColorIncognito() : DimColor();
  if (answerPresent) {
    textLabel.attributedText =
        [self attributedStringWithAnswerLine:match.answer->first_line()];
  } else {
    textLabel.attributedText =
        [self attributedStringWithString:text
                         classifications:textClassifications
                               smallFont:NO
                                   color:suggestionTextColor
                                dimColor:dimColor];
  }
  [textLabel setNeedsDisplay];

  // The leading image (e.g. magnifying glass, star, clock) is only shown on
  // iPad.
  if (IsIPadIdiom()) {
    int imageId = GetIconForAutocompleteMatchType(
        match.type, _popupView->IsStarredMatch(match), _incognito);
    [row updateLeadingImage:imageId];
  }

  // Show append button for search history/search suggestions/Physical Web as
  // the right control element (aka an accessory element of a table view cell).
  BOOL appendableMatch = match.type == AutocompleteMatchType::SEARCH_HISTORY ||
                         match.type == AutocompleteMatchType::SEARCH_SUGGEST ||
                         match.type == AutocompleteMatchType::PHYSICAL_WEB;
  row.appendButton.hidden = !appendableMatch;
  [row.appendButton cancelTrackingWithEvent:nil];

  // If a right accessory element is present or the text alignment is right
  // aligned, adjust the width to align with the accessory element.
  if (appendableMatch || alignmentRight) {
    LayoutRect layout =
        LayoutRectForRectInBoundingRect(textLabel.frame, self.view.frame);
    layout.size.width -= kAppendButtonWidth;
    textLabel.frame = LayoutRectGetRect(layout);
    layout =
        LayoutRectForRectInBoundingRect(detailTextLabel.frame, self.view.frame);
    layout.size.width -=
        kAppendButtonWidth + (answerImagePresent ? answerImagePadding : 0);
    detailTextLabel.frame = LayoutRectGetRect(layout);
  }

  // Since it's a common use case to type in a left-to-right URL while the
  // device is set to a native RTL language, make sure the left alignment looks
  // good by anchoring the leading edge to the left.
  if (LTRTextInRTLLayout) {
    // This is really a left padding, not a leading padding.
    const CGFloat kLTRTextInRTLLayoutLeftPadding =
        IsIPadIdiom() ? (!IsCompactTablet() ? 176 : 94) : 94;
    CGRect frame = textLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    textLabel.frame = frame;

    frame = detailTextLabel.frame;
    frame.size.width -= kLTRTextInRTLLayoutLeftPadding - frame.origin.x;
    frame.origin.x = kLTRTextInRTLLayoutLeftPadding;
    detailTextLabel.frame = frame;
  }
}

- (NSMutableAttributedString*)attributedStringWithAnswerLine:
    (const SuggestionAnswer::ImageLine&)line {
  NSMutableAttributedString* result =
      [[[NSMutableAttributedString alloc] initWithString:@""] autorelease];

  for (size_t i = 0; i < line.text_fields().size(); i++) {
    const SuggestionAnswer::TextField& field = line.text_fields()[i];
    [result
        appendAttributedString:[self attributedStringWithString:field.text()
                                                           type:field.type()]];
  }

  base::scoped_nsobject<NSAttributedString> spacer(
      [[NSAttributedString alloc] initWithString:@"  "]);
  if (line.additional_text() != nil) {
    const SuggestionAnswer::TextField* field = line.additional_text();
    [result appendAttributedString:spacer];
    [result
        appendAttributedString:[self attributedStringWithString:field->text()
                                                           type:field->type()]];
  }

  if (line.status_text() != nil) {
    const SuggestionAnswer::TextField* field = line.status_text();
    [result appendAttributedString:spacer];
    [result
        appendAttributedString:[self attributedStringWithString:field->text()
                                                           type:field->type()]];
  }

  return result;
}

- (NSAttributedString*)attributedStringWithString:(const base::string16&)string
                                             type:(int)type {
  NSDictionary* attributes = nil;

  const id font = (id)kCTFontAttributeName;
  NSString* foregroundColor = (NSString*)kCTForegroundColorAttributeName;
  const id baselineOffset = (id)NSBaselineOffsetAttributeName;

  // Answer types, sizes and colors specified at http://goto.google.com/ais_api.
  switch (type) {
    case SuggestionAnswer::TOP_ALIGNED:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:12],
        baselineOffset : @10.0f,
        foregroundColor : (id)[UIColor grayColor].CGColor,
      };
      break;
    case SuggestionAnswer::DESCRIPTION_POSITIVE:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16],
        foregroundColor : (id)[UIColor colorWithRed:11 / 255.0
                                              green:128 / 255.0
                                               blue:67 / 255.0
                                              alpha:1.0]
                              .CGColor,
      };
      break;
    case SuggestionAnswer::DESCRIPTION_NEGATIVE:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16],
        foregroundColor : (id)[UIColor colorWithRed:197 / 255.0
                                              green:57 / 255.0
                                               blue:41 / 255.0
                                              alpha:1.0]
                              .CGColor,
      };
      break;
    case SuggestionAnswer::PERSONALIZED_SUGGESTION:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16],
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_MEDIUM:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:20],
      };
      break;
    case SuggestionAnswer::ANSWER_TEXT_LARGE:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:24],
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_SMALL:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:12],
        foregroundColor : (id)[UIColor grayColor].CGColor,
      };
      break;
    case SuggestionAnswer::SUGGESTION_SECONDARY_TEXT_MEDIUM:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:14],
        foregroundColor : (id)[UIColor grayColor].CGColor,
      };
      break;
    case SuggestionAnswer::SUGGESTION:
    // Fall through.
    default:
      attributes = @{
        font : [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:16],
      };
  }

  NSString* unescapedString =
      base::SysUTF16ToNSString(net::UnescapeForHTML(string));
  // TODO(jdonnelly): Remove this tag stripping once the JSON parsing class
  // handles HTML tags.
  unescapedString = [unescapedString stringByReplacingOccurrencesOfString:@"<b>"
                                                               withString:@""];
  unescapedString =
      [unescapedString stringByReplacingOccurrencesOfString:@"</b>"
                                                 withString:@""];

  return [[[NSAttributedString alloc] initWithString:unescapedString
                                          attributes:attributes] autorelease];
}

- (void)updateMatches:(const AutocompleteResult&)result
        withAnimation:(BOOL)animation {
  AutocompleteResult oldResults;
  AutocompleteInput emptyInput;
  oldResults.Swap(&_currentResult);
  _currentResult.CopyOldMatches(emptyInput, result, nil);

  [self layoutRows];

  size_t size = _currentResult.size();
  if (animation && size > 0) {
    [self fadeInRows];
  }
}

- (void)layoutRows {
  size_t size = _currentResult.size();

  [self.tableView reloadData];
  [self.tableView beginUpdates];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupMaterialRow* row = _rows[i];

    if (i < size) {
      const AutocompleteMatch& match =
          ((const AutocompleteResult&)_currentResult).match_at((NSUInteger)i);
      [self updateRow:row withMatch:match];
      row.hidden = NO;
    } else {
      row.hidden = YES;
    }
  }
  [self.tableView endUpdates];

  if (IsIPadIdiom())
    [self updateContentInsetForKeyboard];
}

- (void)keyboardDidShow:(NSNotification*)notification {
  NSDictionary* keyboardInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [keyboardInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  keyboardHeight_ = CurrentKeyboardHeight(keyboardFrameValue);
  if (self.tableView.contentSize.height > 0)
    [self updateContentInsetForKeyboard];
}

- (void)updateContentInsetForKeyboard {
  UIView* toView =
      [UIApplication sharedApplication].keyWindow.rootViewController.view;
  CGRect absoluteRect =
      [self.tableView convertRect:self.tableView.bounds toView:toView];
  CGFloat screenHeight = CurrentScreenHeight();
  CGFloat bottomInset = screenHeight - self.tableView.contentSize.height -
                        keyboardHeight_ - absoluteRect.origin.y -
                        kTopAndBottomPadding * 2;
  bottomInset = MAX(kTopAndBottomPadding, -bottomInset);
  self.tableView.contentInset =
      UIEdgeInsetsMake(kTopAndBottomPadding, 0, bottomInset, 0);
  self.tableView.scrollIndicatorInsets = self.tableView.contentInset;
}

- (void)fadeInRows {
  [CATransaction begin];
  [CATransaction setAnimationTimingFunction:[CAMediaTimingFunction
                                                functionWithControlPoints:
                                                                        0:
                                                                        0:
                                                                      0.2:1]];
  for (size_t i = 0; i < kRowCount; i++) {
    OmniboxPopupMaterialRow* row = _rows[i];
    CGFloat beginTime = (i + 1) * .05;
    CABasicAnimation* transformAnimation =
        [CABasicAnimation animationWithKeyPath:@"transform"];
    [transformAnimation
        setFromValue:[NSValue
                         valueWithCATransform3D:CATransform3DMakeTranslation(
                                                    0, -20, 0)]];
    [transformAnimation
        setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [transformAnimation setDuration:0.5];
    [transformAnimation setBeginTime:beginTime];

    CAAnimation* fadeAnimation = OpacityAnimationMake(0, 1);
    [fadeAnimation setDuration:0.5];
    [fadeAnimation setBeginTime:beginTime];

    [[row layer]
        addAnimation:AnimationGroupMake(@[ transformAnimation, fadeAnimation ])
              forKey:@"animateIn"];
  }
  [CATransaction commit];
}

#pragma mark -
#pragma mark Action for append UIButton

- (void)appendButtonTapped:(id)sender {
  NSUInteger row = [sender tag];
  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(row);
  // Make a defensive copy of |match.contents|, as CopyToOmnibox() will trigger
  // a new round of autocomplete and modify |_currentResult|.
  base::string16 contents(match.contents);
  _popupView->CopyToOmnibox(contents);
}

#pragma mark -
#pragma mark UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Setting the top inset of the scrollView to |kTopAndBottomPadding| causes a
  // one time scrollViewDidScroll to |-kTopAndBottomPadding|.  It's easier to
  // just ignore this one scroll tick.
  if (scrollView.contentOffset.y == 0 - kTopAndBottomPadding)
    return;

  _popupView->DidScroll();
  for (OmniboxPopupMaterialRow* row in _rows.get()) {
    row.highlighted = NO;
  }
}

// Set text alignment for popup cells.
- (void)setTextAlignment:(NSTextAlignment)alignment {
  _alignment = alignment;
}

- (BOOL)isSearchMatch:(const AutocompleteMatch::Type&)type {
  return (type == AutocompleteMatchType::NAVSUGGEST ||
          type == AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED ||
          type == AutocompleteMatchType::SEARCH_HISTORY ||
          type == AutocompleteMatchType::SEARCH_SUGGEST ||
          type == AutocompleteMatchType::SEARCH_OTHER_ENGINE);
}

- (NSMutableAttributedString*)
attributedStringWithString:(NSString*)text
           classifications:(const ACMatchClassifications*)classifications
                 smallFont:(BOOL)smallFont
                     color:(UIColor*)defaultColor
                  dimColor:(UIColor*)dimColor {
  if (text == nil)
    return nil;

  CTFontRef fontRef = smallFont ? _smallFont : _bigFont;

  NSMutableAttributedString* as =
      [[[NSMutableAttributedString alloc] initWithString:text] autorelease];

  // Set the base attributes to the default font and color.
  NSDictionary* dict = [NSDictionary
      dictionaryWithObjectsAndKeys:(id)fontRef, (NSString*)kCTFontAttributeName,
                                   defaultColor.CGColor,
                                   (NSString*)kCTForegroundColorAttributeName,
                                   nil];
  [as addAttributes:dict range:NSMakeRange(0, [text length])];

  if (classifications != NULL) {
    CTFontRef boldFontRef = smallFont ? _smallBoldFont : _bigBoldFont;

    for (ACMatchClassifications::const_iterator i = classifications->begin();
         i != classifications->end(); ++i) {
      const BOOL isLast = (i + 1) == classifications->end();
      const size_t nextOffset = (isLast ? [text length] : (i + 1)->offset);
      const NSInteger location = static_cast<NSInteger>(i->offset);
      const NSInteger length = static_cast<NSInteger>(nextOffset - i->offset);
      // Guard against bad, off-the-end classification ranges due to
      // crbug.com/121703 and crbug.com/131370.
      if (i->offset + length > [text length] || length <= 0)
        break;
      const NSRange range = NSMakeRange(location, length);
      if (0 != (i->style & ACMatchClassification::MATCH)) {
        [as addAttribute:(id)kCTFontAttributeName
                   value:(id)boldFontRef
                   range:range];
      }

      if (0 != (i->style & ACMatchClassification::DIM)) {
        [as addAttribute:(id)kCTForegroundColorAttributeName
                   value:(id)dimColor.CGColor
                   range:range];
      }
    }
  }
  return as;
}

#pragma mark -
#pragma mark Table view delegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.size());
  NSUInteger row = indexPath.row;
  _popupView->OpenURLForRow(row);
}

#pragma mark -
#pragma mark Table view data source

- (CGFloat)tableView:(UITableView*)tableView
    heightForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.size());
  return ((OmniboxPopupMaterialRow*)(_rows[indexPath.row])).rowHeight;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  DCHECK_EQ(0, section);
  return _currentResult.size();
}

// Customize the appearance of table view cells.
- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.size());
  return _rows[indexPath.row];
}

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  // iOS doesn't check -numberOfRowsInSection before checking
  // -canEditRowAtIndexPath in a reload call. If |indexPath.row| is too large,
  // simple return |NO|.
  if ((NSUInteger)indexPath.row >= _currentResult.size())
    return NO;

  const AutocompleteMatch& match =
      ((const AutocompleteResult&)_currentResult).match_at(indexPath.row);
  return match.SupportsDeletion();
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(0U, (NSUInteger)indexPath.section);
  DCHECK_LT((NSUInteger)indexPath.row, _currentResult.size());
  if (editingStyle == UITableViewCellEditingStyleDelete) {
    // The delete button never disappears if you don't call this after a tap.
    // It doesn't seem to be required anywhere else.
    [_rows[indexPath.row] prepareForReuse];
    const AutocompleteMatch& match =
        ((const AutocompleteResult&)_currentResult).match_at(indexPath.row);
    _popupView->DeleteMatch(match);
  }
}

@end
