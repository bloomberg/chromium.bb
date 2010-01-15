// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_CONSTRAINED_WINDOW_MAC_H_
#define CHROME_BROWSER_COCOA_CONSTRAINED_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/tab_contents/constrained_window.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_nsobject.h"

@class BrowserWindowController;
@class GTMWindowSheetController;
@class NSView;
@class NSWindow;
class TabContents;

// Base class for constrained dialog delegates. Never inherit from this
// directly.
class ConstrainedWindowMacDelegate {
 public:
  ConstrainedWindowMacDelegate() : is_sheet_open_(false) { }
  virtual ~ConstrainedWindowMacDelegate();

  // Tells the delegate to either delete itself or set up a task to delete
  // itself later. Note that you MUST close the sheet belonging to your delegate
  // in this method.
  virtual void DeleteDelegate() = 0;

  // Called by the tab controller, you do not need to do anything yourself
  // with this method.
  virtual void RunSheet(GTMWindowSheetController* sheetController,
                        NSView* view) = 0;
 protected:
  // Returns true if this delegate's sheet is currently showing.
  bool is_sheet_open() { return is_sheet_open_; }

 private:
  bool is_sheet_open_;
  void set_sheet_open(bool is_open) { is_sheet_open_ = is_open; }
  friend class ConstrainedWindowMac;
};

// Subclass this for a dialog delegate that displays a system sheet such as
// an NSAlert, an open or save file panel, etc.
class ConstrainedWindowMacDelegateSystemSheet
    : public ConstrainedWindowMacDelegate {
 public:
  ConstrainedWindowMacDelegateSystemSheet(id delegate, SEL didEndSelector)
    : systemSheet_(nil),
      delegate_([delegate retain]),
      didEndSelector_(didEndSelector) { }

 protected:
  void set_sheet(id sheet) { systemSheet_.reset([sheet retain]); }
  id sheet() { return systemSheet_; }

 private:
  virtual void RunSheet(GTMWindowSheetController* sheetController,
                        NSView* view);
  scoped_nsobject<id> systemSheet_;
  scoped_nsobject<id> delegate_;
  SEL didEndSelector_;
};

// Subclass this for a dialog delegate that displays a custom sheet, e.g. loaded
// from a nib file.
class ConstrainedWindowMacDelegateCustomSheet
    : public ConstrainedWindowMacDelegate {
 public:
  ConstrainedWindowMacDelegateCustomSheet()
    : customSheet_(nil),
      delegate_(nil),
      didEndSelector_(NULL) { }

  ConstrainedWindowMacDelegateCustomSheet(id delegate, SEL didEndSelector)
    : customSheet_(nil),
      delegate_([delegate retain]),
      didEndSelector_(didEndSelector) { }

 protected:
  // For when you need to delay initalization after the constructor call.
  void init(NSWindow* sheet, id delegate, SEL didEndSelector) {
    DCHECK(!delegate_.get());
    DCHECK(!didEndSelector_);
    customSheet_.reset([sheet retain]);
    delegate_.reset([delegate retain]);
    didEndSelector_ = didEndSelector;
    DCHECK(delegate_.get());
    DCHECK(didEndSelector_);
  }
  void set_sheet(NSWindow* sheet) { customSheet_.reset([sheet retain]); }
  NSWindow* sheet() { return customSheet_; }

 private:
  virtual void RunSheet(GTMWindowSheetController* sheetController,
                        NSView* view);
  scoped_nsobject<NSWindow> customSheet_;
  scoped_nsobject<id> delegate_;
  SEL didEndSelector_;
};

// Constrained window implementation for the Mac port. A constrained window
// is a per-tab sheet on OS X.
//
// Constrained windows work slightly differently on OS X than on the other
// platforms:
// 1. A constrained window is bound to both a tab and window on OS X.
// 2. The delegate is responsible for closing the sheet again when it is
//    deleted.
class ConstrainedWindowMac : public ConstrainedWindow {
 public:
  virtual ~ConstrainedWindowMac();

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow();
  virtual void CloseConstrainedWindow();

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return owner_; }

  // Returns the window's delegate.
  ConstrainedWindowMacDelegate* delegate() { return delegate_; }

  // Makes the constrained window visible, if it is not yet visible.
  void Realize(BrowserWindowController* controller);

 private:
  friend class ConstrainedWindow;

  ConstrainedWindowMac(TabContents* owner,
                       ConstrainedWindowMacDelegate* delegate);

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* owner_;

  // Delegate that provides the contents of this constrained window.
  ConstrainedWindowMacDelegate* delegate_;

  // Controller of the window that contains this sheet.
  BrowserWindowController* controller_;

  // Stores if |ShowConstrainedWindow()| was called.
  bool should_be_visible_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowMac);
};

#endif  // CHROME_BROWSER_COCOA_CONSTRAINED_WINDOW_MAC_H_

