// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/web_application_info.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield_controller.h"

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
// Bubble. BookmarkAppBubbleView provides views for editing the details to
// create a bookmark app with. Don't create a BookmarkAppBubbleView directly,
// instead use the static ShowBubble method.
class BookmarkAppBubbleView : public views::BubbleDelegateView,
                              public views::ButtonListener,
                              public views::TextfieldController {
 public:
  ~BookmarkAppBubbleView() override;

  static void ShowBubble(
      views::View* anchor_view,
      const WebApplicationInfo& web_app_info,
      const BrowserWindow::ShowBookmarkAppBubbleCallback& callback);

 private:
  // Creates a BookmarkAppBubbleView.
  BookmarkAppBubbleView(
      views::View* anchor_view,
      const WebApplicationInfo& web_app_info,
      const BrowserWindow::ShowBookmarkAppBubbleCallback& callback);

  // Overriden from views::BubbleDelegateView:
  void Init() override;
  views::View* GetInitiallyFocusedView() override;

  // Overridden from views::WidgetDelegate:
  void WindowClosing() override;

  // Overridden from views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void GetAccessibleState(ui::AXViewState* state) override;
  gfx::Size GetMinimumSize() const override;

  // Overridden from views::ButtonListener:
  // Closes the bubble or opens the edit dialog.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;

  // Handle the message when the user presses a button.
  void HandleButtonPressed(views::Button* sender);

  // Update the state of the Add button.
  void UpdateAddButtonState();

  // Get the string ID to use for the bubble title.
  int TitleStringId();

  // Get the trimmed contents of the title text field.
  base::string16 GetTrimmedTitle();

  // The WebApplicationInfo that the user is editing.
  WebApplicationInfo web_app_info_;

  // Whether the user has accepted the dialog.
  bool user_accepted_;

  // The callback to be invoked when the dialog is completed.
  BrowserWindow::ShowBookmarkAppBubbleCallback callback_;

  // Button for removing the bookmark.
  views::LabelButton* add_button_;

  // Button to close the window.
  views::LabelButton* cancel_button_;

  // Checkbox to launch as a window.
  views::Checkbox* open_as_window_checkbox_;

  // Textfield showing the title of the app.
  views::Textfield* title_tf_;

  // Image showing the icon of the app.
  views::ImageView* icon_image_view_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_BOOKMARK_APP_BUBBLE_VIEW_H_
