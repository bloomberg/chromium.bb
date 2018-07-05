// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_ACTIONS_H_
#define IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_ACTIONS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

#include "ios/web/public/test/element_selector.h"
#import "ios/web/public/web_state/web_state.h"

namespace web {

// Action wrapper that performs |action| on the webview of |state|.
// The action will fail (in addition to its own failure modes) if the element
// retrieved by |selector| can't be located, or if it doesn't trigger a
// mousedown event on it inside the webview.
id<GREYAction> WebViewVerifiedActionOnElement(
    WebState* state,
    id<GREYAction> action,
    web::test::ElementSelector selector);

// Executes a longpress on the element selected by |selector| in the webview of
// |state|. If |triggers_context_menu| is true, this gesture is expected to
// cause the context menu to appear, and is not expected to trigger events
// in the webview. If |triggers_context_menu| is false, the converse is true.
// This action doesn't fail if the context menu isn't displayed; calling code
// should check for that separately with a matcher.
id<GREYAction> WebViewLongPressElementForContextMenu(
    WebState* state,
    web::test::ElementSelector selector,
    bool triggers_context_menu);

// Taps on element selected by |selector| in the webview of |state|.
id<GREYAction> WebViewTapElement(WebState* state,
                                 web::test::ElementSelector selector);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_ACTIONS_H_
