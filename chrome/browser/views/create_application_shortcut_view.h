// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/web_applications/web_app.h"
#include "views/controls/label.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace views {
class Checkbox;
class Label;
class Window;
};  // namespace views

class MessageLoop;
class Profile;
class TabContents;

// CreateShortcutView implements a dialog that asks user where to create
// the shortcut for given web app.
class CreateApplicationShortcutView : public views::View,
                                      public views::DialogDelegate,
                                      public views::ButtonListener {
 public:
  explicit CreateApplicationShortcutView(TabContents* tab_contents);
  virtual ~CreateApplicationShortcutView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  // Fetch the largest unprocessed icon.
  // The first largest icon downloaded and decoded successfully will be used.
  void FetchIcon();

  // Callback of icon download.
  void OnIconDownloaded(bool errored, const SkBitmap& image);

  // TabContents of the page that we want to create shortcut.
  TabContents* tab_contents_;

  // UI elements on the dialog.
  views::View* app_info_;
  views::Label* create_shortcuts_label_;
  views::Checkbox* desktop_check_box_;
  views::Checkbox* menu_check_box_;
  views::Checkbox* quick_launch_check_box_;

  // Target shortcut info.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Unprocessed icons from the WebApplicationInfo passed in.
  web_app::IconInfoList unprocessed_icons_;

  // Pending app icon download tracked by us.
  class IconDownloadCallbackFunctor;
  IconDownloadCallbackFunctor* pending_download_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutView);
};

#endif  // CHROME_BROWSER_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
