// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/close_bubble_on_tab_activation_helper.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/box_layout.h"

class Browser;

// TODO(https://crbug.com/934689): Separation of providing content for different
// menus and the UI effort to view it between this class and
// |ProfileChooserView| is in progress.

// This class provides the UI for different menus that are created by user
// clicking the avatar button.
class ProfileMenuViewBase : public content::WebContentsDelegate,
                            public views::BubbleDialogDelegateView,
                            public views::StyledLabelListener {
 public:
  static bool IsShowing();
  static void Hide();

  static ProfileMenuViewBase* GetBubbleForTesting();

  typedef std::vector<std::unique_ptr<views::View>> MenuItems;

 protected:
  ProfileMenuViewBase(views::Button* anchor_button,
                      const gfx::Rect& anchor_rect,
                      gfx::NativeView parent_window,
                      Browser* browser);
  ~ProfileMenuViewBase() override;

  void ResetMenu();
  // Adds a set of menu items, either as a |new_group| (using a separator) or
  // appended to the last added items. Takes ownership of the items and passes
  // them to the underlying view when menu is built using
  // |RepopulateViewFromMenuItems|.
  void AddMenuItems(MenuItems& menu_items, bool new_group);
  void RepopulateViewFromMenuItems();

  Browser* browser() const { return browser_; }

  // Return maximal height for the view after which it becomes scrollable.
  // TODO(crbug.com/870303): remove when a general solution is available.
  int GetMaxHeight() const;

  views::Button* anchor_button() const { return anchor_button_; }

  // TODO(https://crbug.com/934689): Remove menu_width functions and make
  // decision inside this class.
  int menu_width() { return menu_width_; }
  void set_menu_width(int menu_width) { menu_width_ = menu_width; }

 private:
  friend class ProfileChooserViewExtensionsTest;

  // views::BubbleDialogDelegateView:
  void WindowClosing() override;
  void OnNativeThemeChanged(const ui::NativeTheme* native_theme) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  int GetDialogButtons() const override;
  ax::mojom::Role GetAccessibleWindowRole() const override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;

  // views::StyledLabelListener:
  void StyledLabelLinkClicked(views::StyledLabel* label,
                              const gfx::Range& range,
                              int event_flags) override;

  Browser* const browser_;

  int menu_width_;

  // ProfileMenuViewBase takes ownership of all menu_items and passes it to the
  // underlying view when it is created.
  std::vector<MenuItems> menu_item_groups_;

  views::Button* const anchor_button_;

  CloseBubbleOnTabActivationHelper close_bubble_helper_;

  DISALLOW_COPY_AND_ASSIGN(ProfileMenuViewBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_MENU_VIEW_BASE_H_
