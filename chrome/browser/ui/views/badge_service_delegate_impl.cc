// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_WIN)
#include "chrome/browser/ui/views/frame/taskbar_decorator_win.cc"
#endif

void BadgeServiceDelegate::SetBadge(content::WebContents* contents) {
#if defined(OS_WIN)
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  auto* window = browser->window()->GetNativeWindow();
  chrome::DrawNumericTaskbarDecoration(window);
#endif
}

void BadgeServiceDelegate::ClearBadge(content::WebContents* contents) {
#if defined(OS_WIN)
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  // Restore the decoration to whatever it is naturally (either nothing or a
  // profile picture badge).
  browser_view->frame()->GetFrameView()->UpdateTaskbarDecoration();
#endif
}
