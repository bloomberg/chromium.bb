// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_EXTERNAL_APP_OPEN_MAIL_HANDLER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_EXTERNAL_APP_OPEN_MAIL_HANDLER_VIEW_CONTROLLER_H_

#include "ios/chrome/browser/ui/collection_view/collection_view_controller.h"

@class MailtoHandler;
@class MailtoURLRewriter;

// Type of callback block when user selects a mailto:// handler.
typedef void (^OpenMailtoHandlerSelectedHandler)(
    MailtoHandler* _Nonnull handler);

// A view controller object to prompt user to make a selection for which app
// to use to handle a tap on mailto:// URL.
@interface OpenMailHandlerViewController : CollectionViewController

// Initializes the view controller with the |rewriter| object which supplies
// the list of supported apps that can handle a mailto:// URL. When the user
// selects a mail client app, the |selectedCallback| block is called.
- (nonnull instancetype)
initWithRewriter:(nullable MailtoURLRewriter*)rewriter
 selectedHandler:(nullable OpenMailtoHandlerSelectedHandler)selectedHandler
    NS_DESIGNATED_INITIALIZER;

- (nonnull instancetype)init NS_UNAVAILABLE;

- (nonnull instancetype)initWithLayout:(nullable UICollectionViewLayout*)layout
                                 style:(CollectionViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_EXTERNAL_APP_OPEN_MAIL_HANDLER_VIEW_CONTROLLER_H_
