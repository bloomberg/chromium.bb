// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_

#include "base/message_loop/message_pump_dispatcher.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class Checkbox;
class Link;
}

class FirstRunDialog : public views::DialogDelegateView,
                       public views::LinkListener,
                       public base::MessagePumpDispatcher {
 public:
  // Displays the first run UI for reporting opt-in, import data etc.
  // Returns true if the dialog was shown.
  static bool Show(Profile* profile);

 private:
  explicit FirstRunDialog(Profile* profile);
  virtual ~FirstRunDialog();

  // views::DialogDelegate:
  virtual views::View* CreateExtraView() OVERRIDE;
  virtual void OnClosed() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Overridden from MessagePumpDispatcher:
  virtual uint32_t Dispatch(const base::NativeEvent& event) OVERRIDE;

  Profile* profile_;
  views::Checkbox* make_default_;
  views::Checkbox* report_crashes_;

  // Set to false as soon as the user clicks a dialog button; this tells the
  // dispatcher we're done.
  bool should_show_dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIRST_RUN_DIALOG_H_
