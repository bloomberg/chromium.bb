// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class GURL;
class Profile;
class SkBitmap;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace views {
class Checkbox;
class Label;
}

// CreateShortcutViewCommon implements a dialog that asks user where to create
// the shortcut for given web app.  There are two variants of this dialog:
// Shortcuts that load a URL in an app-like window, and shortcuts that load
// a chrome app (the kind you see under "apps" on the new tabs page) in an app
// window.  These are implemented as subclasses of CreateShortcutViewCommon.
class CreateApplicationShortcutView : public views::DialogDelegateView,
                                      public views::ButtonListener {
 public:
  enum DialogLayout {
    // URL shortcuts have an info frame at the top with a thumbnail, title and
    // description.
    DIALOG_LAYOUT_URL_SHORTCUT,

    // App shortcuts don't have an info frame, since they are launched from
    // places where it's clear what app they are from.
    DIALOG_LAYOUT_APP_SHORTCUT
  };

  explicit CreateApplicationShortcutView(Profile* profile);
  ~CreateApplicationShortcutView() override;

  // Initialize the controls on the dialog.
  void InitControls(DialogLayout dialog_layout);

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;

  // Overridden from views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool Accept() override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

 protected:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const base::string16& text, bool checked);

  // Profile in which the shortcuts will be created.
  Profile* profile_;

  // UI elements on the dialog.
  // May be NULL if we are not displaying the app's info.
  views::View* app_info_;
  views::Label* create_shortcuts_label_;
  views::Checkbox* desktop_check_box_;
  views::Checkbox* menu_check_box_;
  views::Checkbox* quick_launch_check_box_;

  // Target shortcut and file handler info.
  std::unique_ptr<web_app::ShortcutInfo> shortcut_info_;
  // If false, the shortcut will be created in the root level of the Start Menu.
  bool create_in_chrome_apps_subdir_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutView);
};

// Create an application shortcut pointing to a URL.
class CreateUrlApplicationShortcutView : public CreateApplicationShortcutView {
 public:
  explicit CreateUrlApplicationShortcutView(content::WebContents* web_contents);
  ~CreateUrlApplicationShortcutView() override;

  bool Accept() override;

 private:
  // Fetch the largest unprocessed icon.
  // The first largest icon downloaded and decoded successfully will be used.
  void FetchIcon();

  // Favicon download callback.
  void DidDownloadFavicon(
      int requested_size,
      int id,
      int http_status_code,
      const GURL& image_url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // The tab whose URL is being turned into an app.
  content::WebContents* web_contents_;

  // Pending app icon download tracked by us.
  int pending_download_id_;

  // Unprocessed icons from the WebApplicationInfo passed in.
  web_app::IconInfoList unprocessed_icons_;

  base::WeakPtrFactory<CreateUrlApplicationShortcutView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreateUrlApplicationShortcutView);
};

// Create an application shortcut pointing to a chrome application.
class CreateChromeApplicationShortcutView
    : public CreateApplicationShortcutView {
 public:
  CreateChromeApplicationShortcutView(
      Profile* profile,
      const extensions::Extension* app,
      const base::Callback<void(bool)>& close_callback);
  ~CreateChromeApplicationShortcutView() override;
  bool Accept() override;
  bool Cancel() override;

 private:
  // Called when the app's ShortcutInfo (with icon) is loaded.
  void OnAppInfoLoaded(std::unique_ptr<web_app::ShortcutInfo> shortcut_info);

  base::Callback<void(bool)> close_callback_;

  base::WeakPtrFactory<CreateChromeApplicationShortcutView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreateChromeApplicationShortcutView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
