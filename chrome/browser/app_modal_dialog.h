// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APP_MODAL_DIALOG_H_
#define CHROME_BROWSER_APP_MODAL_DIALOG_H_

#include <string>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

// Define NativeDialog type as platform specific dialog view.
#if defined(OS_WIN)
class ModalDialogDelegate;
typedef ModalDialogDelegate* NativeDialog;
#elif defined(OS_MACOSX)
typedef void* NativeDialog;
#elif defined(TOOLKIT_USES_GTK)
typedef struct _GtkDialog GtkDialog;
typedef struct _GtkWidget GtkWidget;
typedef int gint;
typedef GtkWidget* NativeDialog;
#endif

class TabContents;
namespace IPC {
class Message;
}

// A controller+model base class for modal dialogs.
class AppModalDialog {
 public:
  // A union of data necessary to determine the type of message box to
  // show. |tab_contents| parameter is optional, if provided that tab will be
  // activated before the modal dialog is displayed.
  AppModalDialog(TabContents* tab_contents, const std::wstring& title);
  virtual ~AppModalDialog();

  // Called by the app modal window queue when it is time to show this window.
  void ShowModalDialog();

  // Returns true if the dialog is still valid. As dialogs are created they are
  // added to the AppModalDialogQueue. When the current modal dialog finishes
  // and it's time to show the next dialog in the queue IsValid is invoked.
  // If IsValid returns false the dialog is deleted and not shown.
  virtual bool IsValid() { return !skip_this_dialog_; }

  /////////////////////////////////////////////////////////////////////////////
  // The following methods are platform specific and should be implemented in
  // the platform specific .cc files.
  // Create the platform specific NativeDialog and display it.  When the
  // NativeDialog is closed, it should call OnAccept or OnCancel to notify the
  // renderer of the user's action.  The NativeDialog is also expected to
  // delete the AppModalDialog associated with it.
  virtual void CreateAndShowDialog();

#if defined(TOOLKIT_USES_GTK)
  virtual void HandleDialogResponse(GtkDialog* dialog, gint response_id) = 0;
  // Callback for dialog response calls, passes results to specialized
  // HandleDialogResponse() implementation.
  static void OnDialogResponse(GtkDialog* dialog, gint response_id,
                               AppModalDialog* app_modal_dialog);
#endif

  // Close the dialog if it is showing.
  virtual void CloseModalDialog();

  // Called by the app modal window queue to activate the window.
  void ActivateModalDialog();

  // Completes dialog handling, shows next modal dialog from the queue.
  void CompleteDialog();

  // Dialog window title.
  std::wstring title() {
    return title_;
  }

  // Helper methods used to query or control the dialog. This is used by
  // automation.
  virtual int GetDialogButtons() = 0;
  virtual void AcceptWindow() = 0;
  virtual void CancelWindow() = 0;

 protected:
  // Cleans up the dialog class.
  virtual void Cleanup();
  // Creates the actual platform specific dialog view class.
  virtual NativeDialog CreateNativeDialog() = 0;

  // A reference to the platform native dialog box.
#if defined(OS_LINUX) || defined(OS_WIN)
  NativeDialog dialog_;
#endif

  // Parent tab contents.
  TabContents* tab_contents_;

  // Information about the message box is held in the following variables.
  std::wstring title_;

  // True if the dialog should no longer be shown, e.g. because the underlying
  // tab navigated away while the dialog was queued.
  bool skip_this_dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppModalDialog);
};

#endif  // CHROME_BROWSER_APP_MODAL_DIALOG_H_
