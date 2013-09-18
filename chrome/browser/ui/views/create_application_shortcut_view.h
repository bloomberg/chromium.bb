// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class FaviconDownloadHelper;
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
  explicit CreateApplicationShortcutView(Profile* profile);
  virtual ~CreateApplicationShortcutView();

  // Initialize the controls on the dialog.
  void InitControls();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 protected:
  // Adds a new check-box as a child to the view.
  views::Checkbox* AddCheckbox(const string16& text, bool checked);

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
  string16 shortcut_menu_subdir_;

  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutView);
};

// Create an application shortcut pointing to a URL.
class CreateUrlApplicationShortcutView : public CreateApplicationShortcutView {
 public:
  explicit CreateUrlApplicationShortcutView(content::WebContents* web_contents);
  virtual ~CreateUrlApplicationShortcutView();

  virtual bool Accept() OVERRIDE;

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
      const base::Closure& close_callback);
  virtual ~CreateChromeApplicationShortcutView();
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

 private:
  void OnShortcutInfoLoaded(
      const ShellIntegration::ShortcutInfo& shortcut_info);

  const extensions::Extension* app_;
  base::Closure close_callback_;

  base::WeakPtrFactory<CreateChromeApplicationShortcutView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CreateChromeApplicationShortcutView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_H_
