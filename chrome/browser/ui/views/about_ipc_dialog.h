// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ABOUT_IPC_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_ABOUT_IPC_DIALOG_H_
#pragma once

#if defined(OS_WIN) && defined(IPC_MESSAGE_LOG_ENABLED)

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>

#include "base/memory/singleton.h"
#include "ipc/ipc_logging.h"
#include "views/controls/button/button.h"
#include "views/controls/table/table_view.h"
#include "views/window/dialog_delegate.h"


class Profile;
namespace views {
class NativeViewHost;
class TextButton;
}  // namespace views

class AboutIPCDialog : public views::DialogDelegate,
                       public views::ButtonListener,
                       public IPC::Logging::Consumer,
                       public views::View {
 public:
  // This dialog is a singleton. If the dialog is already opened, it won't do
  // anything, so you can just blindly call this function all you want.
  static void RunDialog();

  virtual ~AboutIPCDialog();

 private:
  friend struct DefaultSingletonTraits<AboutIPCDialog>;

  AboutIPCDialog();

  // Sets up all UI controls for the dialog.
  void SetupControls();

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual views::View* GetContentsView();
  virtual int GetDialogButtons() const;
  virtual std::wstring GetWindowTitle() const;
  virtual void Layout();

  // IPC::Logging::Consumer implementation.
  virtual void Log(const IPC::LogData& data);

  // views::WindowDelegate (via view::DialogDelegate).
  virtual bool CanResize() const;

  // views::ButtonListener.
  virtual void ButtonPressed(views::Button* button, const views::Event& event);

  WTL::CListViewCtrl message_list_;

  views::TextButton* track_toggle_;
  views::TextButton* clear_button_;
  views::TextButton* filter_button_;
  views::NativeViewHost* table_;

  // Set to true when we're tracking network status.
  bool tracking_;

  DISALLOW_COPY_AND_ASSIGN(AboutIPCDialog);
};

#endif  // OS_WIN && IPC_MESSAGE_LOG_ENABLED

#endif  // CHROME_BROWSER_UI_VIEWS_ABOUT_IPC_DIALOG_H_
