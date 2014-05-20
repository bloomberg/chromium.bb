// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/app_icon_loader.h"
#include "chrome/common/web_application_info.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Profile;

namespace extensions {
class AppIconLoader;
}

namespace gfx {
class ImageSkia;
}

namespace views {
class Checkbox;
class ImageView;
class LabelButton;
class Textfield;
}

// BookmarkAppBubbleView is a view intended to be used as the content of a
// Bubble. BookmarkAppBubbleView provides views for editing the bookmark app it
// is created with. Don't create a BookmarkAppBubbleView directly, instead use
// the static ShowBubble method.
class BookmarkAppBubbleView : public views::BubbleDelegateView,
                              public views::ButtonListener,
                              public extensions::AppIconLoader::Delegate {
 public:
  virtual ~BookmarkAppBubbleView();

  static void ShowBubble(views::View* anchor_view,
                         Profile* profile,
                         const WebApplicationInfo& web_app_info,
                         const std::string& extension_id);

 private:
  // Creates a BookmarkAppBubbleView.
  BookmarkAppBubbleView(views::View* anchor_view,
                        Profile* profile,
                        const WebApplicationInfo& web_app_info,
                        const std::string& extension_id);

  // Overriden from views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual void WindowClosing() OVERRIDE;

  // Overridden from views::View:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;

  // Overridden from views::ButtonListener:
  // Closes the bubble or opens the edit dialog.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from extensions::AppIconLoader::Delegate:
  virtual void SetAppImage(const std::string& id,
                           const gfx::ImageSkia& image) OVERRIDE;

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Sets the title and launch type of the app.
  void ApplyEdits();

  // The bookmark app bubble, if we're showing one.
  static BookmarkAppBubbleView* bookmark_app_bubble_;

  // The profile.
  Profile* profile_;

  // The WebApplicationInfo being used to create the app.
  const WebApplicationInfo web_app_info_;

  // The extension id of the bookmark app.
  const std::string extension_id_;

  // Button for removing the bookmark.
  views::LabelButton* add_button_;

  // Button to close the window.
  views::LabelButton* cancel_button_;

  // Checkbox to launch as a tab.
  views::Checkbox* open_as_tab_checkbox_;

  // Textfield showing the title of the app.
  views::Textfield* title_tf_;

  // Image showing the icon of the app.
  views::ImageView* icon_image_view_;

  // When the destructor is invoked should the app be removed?
  bool remove_app_;

  // Used to load the icon.
  scoped_ptr<extensions::AppIconLoader> app_icon_loader_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_
