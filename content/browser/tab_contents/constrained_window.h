// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#define CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
#pragma once

#include "build/build_config.h"

// The different platform specific subclasses use different delegates for their
// dialogs.
#if defined(OS_WIN)
namespace views {
class WindowDelegate;
class DialogDelegate;
}
typedef views::WindowDelegate ConstrainedWindowDelegate;
typedef views::DialogDelegate ConstrainedDialogDelegate;
#elif defined(OS_MACOSX)
class ConstrainedWindowMacDelegate;
class ConstrainedWindowMacDelegateSystemSheet;
typedef ConstrainedWindowMacDelegate ConstrainedWindowDelegate;
typedef ConstrainedWindowMacDelegateSystemSheet ConstrainedDialogDelegate;
#elif defined(TOOLKIT_USES_GTK)
class ConstrainedWindowGtkDelegate;
typedef ConstrainedWindowGtkDelegate ConstrainedWindowDelegate;
typedef ConstrainedWindowGtkDelegate ConstrainedDialogDelegate;
#endif

class TabContents;

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a TabContents'
//  bounds.
//
class ConstrainedWindow {
 public:
  // Create a Constrained Window that contains a platform specific client
  // area. Typical uses include the HTTP Basic Auth prompt. The caller must
  // provide a delegate to describe the content area and to respond to events.
  static ConstrainedWindow* CreateConstrainedDialog(
      TabContents* owner,
      ConstrainedWindowDelegate* delegate);

  // Makes the Constrained Window visible. Only one Constrained Window is shown
  // at a time per tab.
  virtual void ShowConstrainedWindow() = 0;

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;

  // Sets focus on the Constrained Window.
  virtual void FocusConstrainedWindow() {}

 protected:
  virtual ~ConstrainedWindow() {}
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_CONSTRAINED_WINDOW_H_
