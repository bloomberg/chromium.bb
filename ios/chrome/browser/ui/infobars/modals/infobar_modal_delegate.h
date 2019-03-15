// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate to handle InfobarModal actions.
@protocol InfobarModalDelegate

// Asks the delegate to dismiss the InfobarModal.
- (void)dismissInfobarModal:(UIButton*)sender;

// Called when the InfobarModal "Accept" button was pressed.
- (void)modalInfobarButtonWasPressed:(UIButton*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_MODAL_DELEGATE_H_
