// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class View;
class Widget;
}  // namespace views

namespace ui {
class Event;
}  // namespace ui

class IntentPickerLabelButton;

// A bubble that displays a list of aplications (icons and names), after the
// list we show a pair of buttons which allow the user to remember the selection
// or not. This class comunicates the user's selection with a callback used by
// ArcNavigationThrottle.
//   +--------------------------------+
//   | Open with                      |
//   |                                |
//   | Icon1  Name1                   |
//   | Icon2  Name2                   |
//   |  ...                           |
//   | Icon(N) Name(N)                |
//   |                                |
//   |           [Just once] [Always] |
//   +--------------------------------+

class IntentPickerBubbleView : public views::BubbleDialogDelegateView,
                               public views::ButtonListener,
                               public content::WebContentsObserver {
 public:
  using AppInfo = arc::ArcNavigationThrottle::AppInfo;

  ~IntentPickerBubbleView() override;
  static void ShowBubble(content::WebContents* web_contents,
                         const std::vector<AppInfo>& app_info,
                         const IntentPickerResponse& intent_picker_cb);
  static std::unique_ptr<IntentPickerBubbleView> CreateBubbleView(
      const std::vector<AppInfo>& app_info,
      const IntentPickerResponse& intent_picker_cb,
      content::WebContents* web_contents);

  // views::BubbleDialogDelegateView overrides:
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;

 protected:
  // views::BubbleDialogDelegateView overrides:
  void Init() override;
  base::string16 GetWindowTitle() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;

 private:
  friend class IntentPickerBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, NullIcons);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, NonNullIcons);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, LabelsPtrVectorSize);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, VerifyStartingInkDrop);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, InkDropStateTransition);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, PressButtonTwice);
  IntentPickerBubbleView(const std::vector<AppInfo>& app_info,
                         IntentPickerResponse intent_picker_cb,
                         content::WebContents* web_contents);

  // views::BubbleDialogDelegateView overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Similar to ButtonPressed, except this controls the up/down/right/left input
  // while focusing on the |scroll_view_|.
  void ArrowButtonPressed(int index);

  // views::View overrides:
  gfx::Size GetPreferredSize() const override;

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;

  // ui::EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Retrieves the IntentPickerLabelButton* contained at position |index| from
  // the internal ScrollView.
  IntentPickerLabelButton* GetIntentPickerLabelButtonAt(size_t index);
  void RunCallback(std::string package,
                   arc::ArcNavigationThrottle::CloseReason close_reason);

  // Accessory for |scroll_view_|'s contents size.
  size_t GetScrollViewSize() const;

  // Ensure the selected app is within the visible region of the ScrollView.
  void AdjustScrollViewVisibleRegion();

  // Set the new app selection, use the |event| (if provided) to show a more
  // accurate ripple effect w.r.t. the user's input.
  void SetSelectedAppIndex(int index, const ui::Event* event);

  // Calculate the next app to select given the current selection and |delta|.
  size_t CalculateNextAppIndex(int delta);

  gfx::ImageSkia GetAppImageForTesting(size_t index);
  views::InkDropState GetInkDropStateForTesting(size_t);
  void PressButtonForTesting(size_t index, const ui::Event& event);

  // Callback used to respond to ArcNavigationThrottle.
  IntentPickerResponse intent_picker_cb_;

  // Pre-select the first app on the list.
  size_t selected_app_tag_ = 0;

  views::ScrollView* scroll_view_;

  std::vector<AppInfo> app_info_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
