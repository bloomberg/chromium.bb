// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "build/build_config.h"
#include "views/views_delegate.h"

namespace views {
class Window;
}

class ChromeViewsDelegate : public views::ViewsDelegate {
 public:
  ChromeViewsDelegate() {}
  virtual ~ChromeViewsDelegate() {}

  // Overridden from views::ViewsDelegate:
  virtual ui::Clipboard* GetClipboard() const;
  virtual void SaveWindowPlacement(views::Window* window,
                                   const std::wstring& window_name,
                                   const gfx::Rect& bounds,
                                   bool maximized);
  virtual bool GetSavedWindowBounds(views::Window* window,
                                    const std::wstring& window_name,
                                    gfx::Rect* bounds) const;
  virtual bool GetSavedMaximizedState(views::Window* window,
                                      const std::wstring& window_name,
                                      bool* maximized) const;
  virtual void NotifyAccessibilityEvent(
      views::View* view, AccessibilityTypes::Event event_type);
#if defined(OS_WIN)
  virtual HICON GetDefaultWindowIcon() const;
#endif
  virtual void AddRef();
  virtual void ReleaseRef();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeViewsDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_VIEWS_DELEGATE_H_
