// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_

#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"

// Contains the model data for a language in the Language Settings page.
@interface LanguageItem : TableViewMultiDetailTextItem

// The language code for this language.
@property(nonatomic, assign) std::string languageCode;

// Whether the language is Translate-blocked.
@property(nonatomic, getter=isBlocked) BOOL blocked;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_LANGUAGE_ITEM_H_
