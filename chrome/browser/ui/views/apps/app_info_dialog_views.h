// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_

#include "base/compiler_specific.h"
#include "chrome/browser/shell_integration.h"
#include "ui/message_center/views/bounded_scroll_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Label;
}

// A scrollable list of permissions for the given app.
class PermissionsScrollView : public message_center::BoundedScrollView {
 public:
  PermissionsScrollView(int min_height,
                        int max_height,
                        const extensions::Extension* app);

 private:
  virtual ~PermissionsScrollView();

  views::View* inner_scrollable_view;
};

// View the information about a particular chrome application.
class AppInfoView : public views::DialogDelegateView,
                    public base::SupportsWeakPtr<AppInfoView> {
 public:
  AppInfoView(Profile* profile,
              const extensions::Extension* app,
              const base::Closure& close_callback);

 private:
  virtual ~AppInfoView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual bool Cancel() OVERRIDE;
  virtual base::string16 GetDialogButtonLabel(ui::DialogButton button)
      const OVERRIDE;
  virtual int GetDialogButtons() const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual ui::ModalType GetModalType() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;

  // Called when the app's icon is loaded.
  void OnAppImageLoaded(const gfx::Image& image);

  // Profile in which the shortcuts will be created.
  Profile* profile_;

  // UI elements on the dialog.
  views::Label* app_name_label;
  views::Label* app_description_label;
  views::Label* app_version_label;
  views::ImageView* app_icon;
  views::Label* permission_list_heading;
  PermissionsScrollView* permissions_scroll_view;

  const extensions::Extension* app_;
  base::Closure close_callback_;

  base::WeakPtrFactory<AppInfoView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_VIEWS_H_
