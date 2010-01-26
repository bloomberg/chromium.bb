// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_COOKIE_PROMPT_VIEW_H_
#define CHROME_BROWSER_VIEWS_COOKIE_PROMPT_VIEW_H_

#include <string>

#include "base/task.h"
#include "chrome/browser/views/cookie_info_view.h"
#include "net/base/cookie_monster.h"
#include "views/controls/button/button.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace views {
class Label;
class NativeButton;
class RadioButton;
}

class CookieInfoView;
class Profile;
class Timer;

class CookiesPromptViewDelegate {
 public:
  // Allow cookie to be set. If |remember| is true, record this decision
  // for this host.
  virtual void AllowCookie(bool remember, bool session_expire) = 0;

  // Block cookie from being stored. If |remember| is true, record this
  // decision for this host.
  virtual void BlockCookie(bool remember) = 0;

 protected:
  virtual ~CookiesPromptViewDelegate() {}
};

// Cookie alert dialog UI.
class CookiesPromptView : public views::View,
                          public views::DialogDelegate,
                          public views::ButtonListener,
                          public views::LinkController,
                          public CookieInfoViewDelegate {
 public:
  // Show the Cookies Window, creating one if necessary.
  static void ShowCookiePromptWindow(
      gfx::NativeWindow parent,
      Profile* profile,
      const std::string& domain,
      const net::CookieMonster::CanonicalCookie& cookie,
      CookiesPromptViewDelegate* delegate);

  virtual ~CookiesPromptView();

  void SetCookie(const std::string& domain,
                 const net::CookieMonster::CanonicalCookie& cookie_node);

 protected:
  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // views::DialogDelegate overrides.
  virtual bool CanResize() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();

  // views::ButtonListener overrides.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::LinkController overrides.
  virtual void LinkActivated(views::Link* source, int event_flags);

  // views::WindowDelegate overrides.
  virtual int GetDialogButtons() const {
    return MessageBoxFlags::DIALOGBUTTON_NONE;
  }

  // CookieInfoViewDelegate overrides:
  virtual void ModifyExpireDate(bool session_expire);

 private:
  // Use the static factory method to show.
  explicit CookiesPromptView(Profile* profile,
                             CookiesPromptViewDelegate* delegate);

  // Initialize the dialog layout.
  void Init();

  // Shows or hides cooke info view and changes parent.
  void ToggleCookieViewExpand();

  // Calculates view size offset depending on visibility of cookie details.
  int GetExtendedViewHeight();

  views::Label* description_label_;
  views::RadioButton* remember_radio_;
  views::RadioButton* ask_radio_;
  views::NativeButton* allow_button_;
  views::NativeButton* block_button_;
  views::Link* show_cookie_link_;
  views::Link* manage_cookies_link_;
  CookieInfoView* info_view_;

  // True if cookie should expire with this session.
  bool session_expire_;

  // True if cookie info view is currently shown and window expanded.
  bool expanded_view_;

  // True if the outcome of this dialog has been signaled to the delegate.
  bool signaled_;

  // Cookie prompt window title.
  std::wstring title_;

  CookiesPromptViewDelegate* delegate_;

  // Cookie domain.
  std::string domain_;

  // Cookie domain formatted for displaying (removed leading '.').
  std::wstring display_domain_;

  // Displayed cookie.
  scoped_ptr<net::CookieMonster::CanonicalCookie> cookie_;

  // The Profile for which Cookies are displayed.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(CookiesPromptView);
};

#endif  // CHROME_BROWSER_VIEWS_COOKIE_PROMPT_VIEW_H_

