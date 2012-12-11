// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>
#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/browser_window_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/password_form.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/native_widget_types.h"

typedef InProcessBrowserTest BrowserWindowCocoaBrowsertest;

IN_PROC_BROWSER_TEST_F(BrowserWindowCocoaBrowsertest,
                       ShowPasswordGenerationBubble) {
  gfx::Rect rect;
  content::PasswordForm form;
  autofill::PasswordGenerator generator(10);
  browser()->window()->ShowPasswordGenerationBubble(rect, form, &generator);

  // Make sure that the password generation bubble is actually created.
  NSWindow* window = browser()->window()->GetNativeWindow();
  BOOL found = NO;
  for (NSUInteger i = 0; i < [[window childWindows] count]; i++) {
    id object = [[window childWindows] objectAtIndex:i];
    if ([object isKindOfClass:[InfoBubbleWindow class]])
      found = YES;
  }
  EXPECT_TRUE(found);
}
