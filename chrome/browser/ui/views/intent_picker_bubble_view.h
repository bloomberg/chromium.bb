// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/button/button.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace views {
class EventMonitor;
class Label;
class LabelButton;
class View;
class Widget;
}  // namespace views

namespace ui {
class Event;
}  // namespace ui

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
//   |           [JUST ONCE] [ALWAYS] |
//   +--------------------------------+

class IntentPickerBubbleView : public views::BubbleDialogDelegateView,
                               public views::ButtonListener,
                               public content::WebContentsObserver {
 public:
  using NameAndIcon = arc::ArcNavigationThrottle::NameAndIcon;
  // This callback informs the index of the app selected by the user, along with
  // the reason why the Bubble was closed. The size_t param must have a value in
  // the range [0, app_info.size()-1], except when the CloseReason is ERROR or
  // DIALOG_DEACTIVATED, for these cases we return a dummy value
  // |kAppTagNoneSelected| which won't be used at all and has no significance.
  using ThrottleCallback =
      base::Callback<void(size_t, arc::ArcNavigationThrottle::CloseReason)>;

  static void ShowBubble(content::NavigationHandle* handle,
                         const std::vector<NameAndIcon>& app_info,
                         const ThrottleCallback& throttle_cb);

 protected:
  // views::BubbleDialogDelegateView overrides:
  void Init() override;

 private:
  IntentPickerBubbleView(const std::vector<NameAndIcon>& app_info,
                         ThrottleCallback throttle_cb,
                         content::WebContents* web_contents);
  ~IntentPickerBubbleView() override;

  // views::BubbleDialogDelegateView overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  int GetDialogButtons() const override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View overrides:
  gfx::Size GetPreferredSize() const override;

  // content::WebContentsObserver overrides:
  void WebContentsDestroyed() override;

  // Flag set to true iff the callback was Run at some previous step, used to
  // ensure we only use the callback once.
  bool was_callback_run_;

  // Callback used to respond to ArcNavigationThrottle.
  ThrottleCallback throttle_cb_;

  // Keeps a invalid value unless the user explicitly makes a decision.
  size_t selected_app_tag_;

  views::LabelButton* always_button_;
  views::LabelButton* just_once_button_;

  std::vector<NameAndIcon> app_info_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
