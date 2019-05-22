// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_icon_formatter.h"

#import "components/omnibox/browser/autocomplete_match.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/images/branded_image_provider.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

OmniboxSuggestionIconType IconTypeFromMatchAndAnswerType(
    AutocompleteMatchType::Type type,
    base::Optional<int> answerType) {
  if (answerType) {
    switch (answerType.value()) {
      case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
        return DICTIONARY;
      case SuggestionAnswer::ANSWER_TYPE_FINANCE:
        return STOCK;
      case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
        return TRANSLATION;
      case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
        return WHEN_IS;
      case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
        return CONVERSION;
      case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
        return SUNRISE;
      case SuggestionAnswer::ANSWER_TYPE_KNOWLEDGE_GRAPH:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL:
      case SuggestionAnswer::ANSWER_TYPE_SPORTS:
      case SuggestionAnswer::ANSWER_TYPE_LOCAL_TIME:
      case SuggestionAnswer::ANSWER_TYPE_PLAY_INSTALL:
      case SuggestionAnswer::ANSWER_TYPE_WEATHER:
        return FALLBACK_ANSWER;
      case SuggestionAnswer::ANSWER_TYPE_INVALID:
      case SuggestionAnswer::ANSWER_TYPE_TOTAL_COUNT:
        NOTREACHED();
        break;
    }
  }
  switch (type) {
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
      return DEFAULT_FAVICON;
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return SEARCH;
    case AutocompleteMatchType::CALCULATOR:
      return CALCULATOR;
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      return DEFAULT_FAVICON;
  }
}

}  // namespace

@interface OmniboxIconFormatter ()

@property(nonatomic, assign) BOOL isAnswer;
@property(nonatomic, assign) OmniboxIconType iconType;
@property(nonatomic, assign) OmniboxSuggestionIconType suggestionIconType;
@property(nonatomic, assign) GURL imageURL;

@end

@implementation OmniboxIconFormatter

- (instancetype)initWithMatch:(const AutocompleteMatch&)match {
  self = [super init];
  if (self) {
    _isAnswer = match.answer.has_value();
    if (_isAnswer && match.answer->second_line().image_url().is_valid()) {
      _iconType = OmniboxIconTypeImage;
      _imageURL = match.answer->second_line().image_url();
    } else if (!match.image_url.empty()) {
      _iconType = OmniboxIconTypeImage;
      _imageURL = GURL(match.image_url);
    } else if (!AutocompleteMatch::IsSearchType(match.type) &&
               !match.destination_url.is_empty()) {
      _iconType = OmniboxIconTypeFavicon;
      _imageURL = match.destination_url;
    } else {
      _iconType = OmniboxIconTypeSuggestionIcon;
      _imageURL = GURL();
    }

    auto answerType = _isAnswer ? base::make_optional<int>(match.answer->type())
                                : base::nullopt;
    _suggestionIconType =
        IconTypeFromMatchAndAnswerType(match.type, answerType);
  }
  return self;
}

- (UIImage*)iconImage {
  if (self.suggestionIconType == FALLBACK_ANSWER &&
      self.defaultSearchEngineIsGoogle && [self fallbackAnswerBrandedIcon]) {
    return [[self fallbackAnswerBrandedIcon]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
  return GetOmniboxSuggestionIcon(self.suggestionIconType);
}

- (BOOL)hasCustomAnswerIcon {
  switch (self.suggestionIconType) {
    case BOOKMARK:
    case DEFAULT_FAVICON:
    case HISTORY:
    case SEARCH:
      return NO;
    case CALCULATOR:
    case CONVERSION:
    case DICTIONARY:
    case STOCK:
    case SUNRISE:
    case LOCAL_TIME:
    case WHEN_IS:
    case TRANSLATION:
      return YES;
    // For the fallback answer, this depends on whether the branded icon exists
    // and whether the default search engine is Google (the icon only exists for
    // Google branding).
    // The default fallback answer icon uses the grey background styling, like
    // the non-answer icons.
    case FALLBACK_ANSWER:
      return self.defaultSearchEngineIsGoogle &&
             [self fallbackAnswerBrandedIcon];
    case OMNIBOX_SUGGESTION_ICON_TYPE_COUNT:
      NOTREACHED();
      return NO;
  }
}

- (UIImage*)fallbackAnswerBrandedIcon {
  return ios::GetChromeBrowserProvider()
      ->GetBrandedImageProvider()
      ->GetOmniboxAnswerIcon();
}

- (UIColor*)iconImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return UIColor.whiteColor;
      }
      return [UIColor.blackColor colorWithAlphaComponent:0.5];
    case OmniboxIconTypeFavicon:
      return [UIColor.blackColor colorWithAlphaComponent:0.5];
  }
}

- (UIImage*)backgroundImage {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return nil;
    case OmniboxIconTypeSuggestionIcon:
      return [[UIImage imageNamed:@"background_solid"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    case OmniboxIconTypeFavicon:
      return [[UIImage imageNamed:@"background_stroke"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  }
}

- (UIColor*)backgroundImageTintColor {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return nil;
    case OmniboxIconTypeSuggestionIcon:
      if ([self hasCustomAnswerIcon]) {
        return [MDCPalette.cr_bluePalette tint500];
      }
      return [UIColor.blackColor colorWithAlphaComponent:0.1];
    case OmniboxIconTypeFavicon:
      return [UIColor.blackColor colorWithAlphaComponent:0.2];
  }
}

- (UIImage*)overlayImage {
  switch (self.iconType) {
    case OmniboxIconTypeImage:
      return self.isAnswer ? nil
                           : [[UIImage imageNamed:@"background_stroke"]
                                 imageWithRenderingMode:
                                     UIImageRenderingModeAlwaysTemplate];
    case OmniboxIconTypeSuggestionIcon:
    case OmniboxIconTypeFavicon:
      return nil;
  }
}

- (UIColor*)overlayImageTintColor {
  return [UIColor.blackColor colorWithAlphaComponent:0.1];
}

@end
