// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/image_loading_tracker.h"
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

class Extension;
class MessageLoop;
class Profile;
class TabContents;

// CreateShortcutViewCommon implements a dialog that asks user where to create
// the shortcut for given web app.  There are two variants of this dialog:
// Shortcuts that load a URL in an app-like window, and shortcuts that load
// a chrome app (the kind you see under "apps" on the new tabs page) in an app
// window.  These are implemented as subclasses of CreateShortcutViewCommon.
class CreateApplicationShortcutView : public views::View,
                                      public views::DialogDelegate,
                                      public views::ButtonListener {
 public:
  explicit CreateApplicationShortcutView(Profile* profile);
  virtual ~CreateApplicationShortcutView();

  // Initialize the controls on the dialog.
  void InitControls();

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

 protected:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const std::wstring& text, bool checked);

  // Profile in which the shortcuts will be created.
  Profile* profile_;

  // UI elements on the dialog.
  views::View* app_info_;
  views::Label* create_shortcuts_label_;
  views::Checkbox* desktop_check_box_;
  views::Checkbox* menu_check_box_;
  views::Checkbox* quick_launch_check_box_;

  // Target shortcut info.
  ShellIntegration::ShortcutInfo shortcut_info_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutView);
};

// Create an application shortcut pointing to a URL.
class CreateUrlApplicationShortcutView : public CreateApplicationShortcutView {
 public:
  explicit CreateUrlApplicationShortcutView(TabContents* tab_contents);
  virtual ~CreateUrlApplicationShortcutView();

  virtual bool Accept();

 private:
  // Fetch the largest unprocessed icon.
  // The first largest icon downloaded and decoded successfully will be used.
  void FetchIcon();

  // Callback of icon download.
  void OnIconDownloaded(bool errored, const SkBitmap& image);

  // The tab whose URL is being turned into an app.
  TabContents* tab_contents_;

  // Pending app icon download tracked by us.
  class IconDownloadCallbackFunctor;
  IconDownloadCallbackFunctor* pending_download_;

  // Unprocessed icons from the WebApplicationInfo passed in.
  web_app::IconInfoList unprocessed_icons_;

  DISALLOW_COPY_AND_ASSIGN(CreateUrlApplicationShortcutView);
};

// Create an application shortcut pointing to a chrome application.
class CreateChromeApplicationShortcutView
   : public CreateApplicationShortcutView,
     public ImageLoadingTracker::Observer {
 public:
  explicit CreateChromeApplicationShortcutView(Profile* profile,
                                               const Extension* app);
  virtual ~CreateChromeApplicationShortcutView();

  // Implement ImageLoadingTracker::Observer.  |tracker_| is used to
  // load the app's icon.  This method recieves the icon, and adds
  // it to the "Create Shortcut" dailog box.
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int index);

 private:
  const Extension* app_;
  ImageLoadingTracker tracker_;

  DISALLOW_COPY_AND_ASSIGN(CreateChromeApplicationShortcutView);
};


#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
