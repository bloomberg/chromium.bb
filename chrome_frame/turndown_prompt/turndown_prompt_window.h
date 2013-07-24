// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_WINDOW_H_
#define CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_WINDOW_H_

#include <windows.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlcrack.h>
#include <atlframe.h>
#include <atltheme.h>
#include <atlwin.h>

#include "base/debug/debugger.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_handle.h"
#include "chrome_frame/infobars/infobar_content.h"
#include "chrome_frame/resource.h"
#include "grit/chrome_frame_dialogs.h"

class UrlLauncher;

namespace WTL {
class CHyperLink;
class CBitmapButton;
}  // namespace WTL

class CFBitmapButton;

// Implements a dialog with text and buttons notifying the user that Chrome
// Frame is being turned down, offering them a link to learn more about moving
// to a modern browser.
class TurndownPromptWindow
    : public CDialogImpl<TurndownPromptWindow, CWindow>,
      public CDialogResize<TurndownPromptWindow>,
      public CThemeImpl<TurndownPromptWindow> {
 public:
  enum { IDD = IDD_CHROME_FRAME_TURNDOWN_PROMPT };

  // Creates and initializes a dialog for display in the provided frame.  The
  // UrlLauncher may be used to launch a new tab containing a knowledge-base
  // article about the turndown.
  //
  // Upon success, takes ownership of itself (to be deleted upon WM_DESTROY) and
  // returns a weak pointer to this dialog. Upon failure, returns a null weak
  // pointer and deletes self.
  //
  // In either case, takes ownership of the UrlLauncher but not the frame.
  // |uninstall_closure| is invoked if/when the Uninstall button is activated.
  static base::WeakPtr<TurndownPromptWindow> CreateInstance(
      InfobarContent::Frame* frame,
      UrlLauncher* url_launcher,
      const base::Closure& uninstall_closure);

  BEGIN_MSG_MAP(InfobarWindow)
    CHAIN_MSG_MAP(CThemeImpl<TurndownPromptWindow>)
    MSG_WM_DESTROY(OnDestroy)
    MSG_WM_INITDIALOG(OnInitDialog)
    NOTIFY_HANDLER(IDC_TD_PROMPT_LINK, NM_CLICK, OnLearnMore)
    COMMAND_HANDLER(IDUNINSTALL, BN_CLICKED, OnUninstall)
    COMMAND_HANDLER(IDDISMISS, BN_CLICKED, OnDismiss)
    CHAIN_MSG_MAP(CDialogResize<TurndownPromptWindow>)
  END_MSG_MAP()

  BEGIN_DLGRESIZE_MAP(InfobarWindow)
    DLGRESIZE_CONTROL(IDDISMISS, DLSZ_CENTER_Y | DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDUNINSTALL, DLSZ_CENTER_Y | DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_TD_PROMPT_LINK, DLSZ_CENTER_Y | DLSZ_MOVE_X)
    DLGRESIZE_CONTROL(IDC_TD_PROMPT_MESSAGE, DLSZ_SIZE_Y | DLSZ_SIZE_X)
  END_DLGRESIZE_MAP()

  virtual void OnFinalMessage(HWND);

 private:
  // Call CreateInstance() to get an initialized TurndownPromptWindow.
  TurndownPromptWindow(InfobarContent::Frame* frame,
                       UrlLauncher* url_launcher,
                       const base::Closure& uninstall_closure);

  // The TurndownPromptWindow manages its own destruction.
  virtual ~TurndownPromptWindow();

  // Performs the necessary configuration to initialize a bitmap button.
  static void SetupBitmapButton(TurndownPromptWindow* window);

  // Event handlers.
  void OnDestroy();
  BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
  LRESULT OnLearnMore(WORD wParam, LPNMHDR lParam, BOOL& bHandled);  // NOLINT
  LRESULT OnUninstall(WORD wNotifyCode,
                      WORD wID,
                      HWND hWndCtl,
                      BOOL& bHandled);
  LRESULT OnDismiss(WORD wNotifyCode,
                    WORD wID,
                    HWND hWndCtl,
                    BOOL& bHandled);

  // Returns the prompt text for the current version of IE.
  static string16 GetPromptText();

  InfobarContent::Frame* frame_;  // Not owned by this instance
  scoped_ptr<WTL::CHyperLink> link_;
  scoped_ptr<CFBitmapButton> close_button_;
  scoped_ptr<UrlLauncher> url_launcher_;
  base::Closure uninstall_closure_;

  base::WeakPtrFactory<TurndownPromptWindow> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(TurndownPromptWindow);
};  // class TurndownPromptWindow

#endif  // CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_WINDOW_H_
