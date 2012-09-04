// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/constrained_window.h"

class ConstrainedWindowMac;
@class GTMWindowSheetController;
@class NSView;
@class NSWindow;
class TabContents;

// Window controllers that allow hosting constrained windows should
// implement this protocol.
@protocol ConstrainedWindowSupport

- (GTMWindowSheetController*)sheetController;

@end

// Base class for constrained dialog delegates. Never inherit from this
// directly.
class ConstrainedWindowMacDelegate {
 public:
  ConstrainedWindowMacDelegate() : is_sheet_open_(false) {}
  virtual ~ConstrainedWindowMacDelegate() {}

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
  ConstrainedWindowMacDelegateSystemSheet(id delegate, SEL didEndSelector);
  virtual ~ConstrainedWindowMacDelegateSystemSheet();

 protected:
  void set_sheet(id sheet);
  id sheet() { return systemSheet_; }

  // Returns an NSArray to be passed as parameters to GTMWindowSheetController.
  // Array's contents should be the arguments passed to the system sheet's
  // beginSheetForWindow:... method. The window argument must be [NSNull null].
  //
  // The default implementation returns
  //     [null window, delegate, didEndSelector, null contextInfo]
  // Subclasses may override this if they show a system sheet which takes
  // different parameters.
  virtual NSArray* GetSheetParameters(id delegate, SEL didEndSelector);

 private:
  friend class TabModalConfirmDialogTest;

  virtual void RunSheet(GTMWindowSheetController* sheetController,
                        NSView* view) OVERRIDE;
  scoped_nsobject<id> systemSheet_;
  scoped_nsobject<id> delegate_;
  SEL didEndSelector_;
};

// Subclass this for a dialog delegate that displays a custom sheet, e.g. loaded
// from a nib file.
class ConstrainedWindowMacDelegateCustomSheet
    : public ConstrainedWindowMacDelegate {
 public:
  ConstrainedWindowMacDelegateCustomSheet();
  ConstrainedWindowMacDelegateCustomSheet(id delegate, SEL didEndSelector);
  virtual ~ConstrainedWindowMacDelegateCustomSheet();

 protected:
  // For when you need to delay initalization after the constructor call.
  void init(NSWindow* sheet, id delegate, SEL didEndSelector);
  void set_sheet(NSWindow* sheet);
  NSWindow* sheet() { return customSheet_; }

 private:
  virtual void RunSheet(GTMWindowSheetController* sheetController,
                        NSView* view) OVERRIDE;
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
  ConstrainedWindowMac(TabContents* tab_contents,
                       ConstrainedWindowMacDelegate* delegate);
  virtual ~ConstrainedWindowMac();

  // Overridden from ConstrainedWindow:
  virtual void ShowConstrainedWindow() OVERRIDE;
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual bool CanShowConstrainedWindow() OVERRIDE;

  // Returns the TabContents that constrains this Constrained Window.
  TabContents* owner() const { return tab_contents_; }

  // Returns the window's delegate.
  ConstrainedWindowMacDelegate* delegate() { return delegate_; }

  // Makes the constrained window visible, if it is not yet visible.
  void Realize(NSWindowController<ConstrainedWindowSupport>* controller);

 private:
  friend class ConstrainedWindow;

  // The TabContents that owns and constrains this ConstrainedWindow.
  TabContents* tab_contents_;

  // Delegate that provides the contents of this constrained window.
  ConstrainedWindowMacDelegate* delegate_;

  // Controller of the window that contains this sheet.
  NSWindowController<ConstrainedWindowSupport>* controller_;

  // Stores if |ShowConstrainedWindow()| was called.
  bool should_be_visible_;

  // True when CloseConstrainedWindow has been called.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_MAC_H_
