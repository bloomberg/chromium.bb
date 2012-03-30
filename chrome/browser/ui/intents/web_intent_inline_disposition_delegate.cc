// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_inline_disposition_delegate.h"

#include "base/logging.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

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

content::WebContents* WebIntentInlineDispositionDelegate::OpenURLFromTab(
    content::WebContents* source, const content::OpenURLParams& params) {
  DCHECK(source);  // Can only be invoked from inline disposition.

  // Load in place.
  source->GetController().LoadURL(params.url, content::Referrer(),
      content::PAGE_TRANSITION_START_PAGE, std::string());

  // Remove previous history entries - users should not navigate in intents.
  source->GetController().PruneAllButActive();

  return source;
}
