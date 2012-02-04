// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"

WebIntentInlineDispositionDelegate::WebIntentInlineDispositionDelegate() {
}

WebIntentInlineDispositionDelegate::~WebIntentInlineDispositionDelegate() {
}

bool WebIntentInlineDispositionDelegate::IsPopupOrPanel(
    const content::WebContents* source) const {
  return true;
}

bool WebIntentInlineDispositionDelegate::ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  return false;
}
