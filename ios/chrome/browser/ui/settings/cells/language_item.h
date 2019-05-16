// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"

#include <string>

// Contains the model data for a language in the Language Settings page.
@interface LanguageItem : TableViewMultiDetailTextItem

// The language code for this language.
@property(nonatomic, assign) std::string languageCode;

// The language code for this language used by the Translate server.
@property(nonatomic, assign) std::string canonicalLanguageCode;

// Whether the language is Translate-blocked.
@property(nonatomic, getter=isBlocked) BOOL blocked;

// Whether the language is supported by the Translate server.
@property(nonatomic) BOOL supportsTranslate;

// Whether Translate can be offered for the language (it can be unblocked). This
// must be false if |supportsTranslate| is false. It can however be false for
// other reasons such as the language being the last Translate-blocked language.
@property(nonatomic) BOOL canOfferTranslate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_
