// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/scroll_view.h"

namespace {

// Helpers --------------------------------------------------------------------

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         const gfx::Rect& anchor_rect,
                                         gfx::NativeView parent_window,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      menu_width_(0),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  // TODO(sajadm): Remove when fixing https://crbug.com/822075
  // The sign in webview will be clipped on the bottom corners without these
  // margins, see related bug <http://crbug.com/593203>.
  set_margins(gfx::Insets(0, 0, 2, 0));
  if (anchor_button) {
    anchor_button->AnimateInkDrop(views::InkDropState::ACTIVATED, nullptr);
  } else {
    DCHECK(parent_window);
    SetAnchorRect(anchor_rect);
    set_parent_window(parent_window);
  }

  // The arrow keys can be used to tab between items.
  AddAccelerator(ui::Accelerator(ui::VKEY_DOWN, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_UP, ui::EF_NONE));
}

ProfileMenuViewBase::~ProfileMenuViewBase() = default;

void ProfileMenuViewBase::ShowMenu() {
  views::BubbleDialogDelegateView::CreateBubble(this)->Show();
}

void ProfileMenuViewBase::OnNativeThemeChanged(
    const ui::NativeTheme* native_theme) {
  views::BubbleDialogDelegateView::OnNativeThemeChanged(native_theme);
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_DialogBackground)));
}

bool ProfileMenuViewBase::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_DOWN &&
      accelerator.key_code() != ui::VKEY_UP)
    return BubbleDialogDelegateView::AcceleratorPressed(accelerator);

  // Move the focus up or down.
  GetFocusManager()->AdvanceFocus(accelerator.key_code() != ui::VKEY_DOWN);
  return true;
}

int ProfileMenuViewBase::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ProfileMenuViewBase::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  // Suppresses the context menu because some features, such as inspecting
  // elements, are not appropriate in a bubble.
  return true;
}

void ProfileMenuViewBase::StyledLabelLinkClicked(views::StyledLabel* label,
                                                 const gfx::Range& range,
                                                 int event_flags) {
  chrome::ShowSettings(browser_);
}

int ProfileMenuViewBase::GetMaxHeight() const {
  gfx::Rect anchor_rect = GetAnchorRect();
  gfx::Rect screen_space =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(anchor_rect.CenterPoint())
          .work_area();
  int available_space = screen_space.bottom() - anchor_rect.bottom();
#if defined(OS_WIN)
  // On Windows the bubble can also be show to the top of the anchor.
  available_space =
      std::max(available_space, anchor_rect.y() - screen_space.y());
#endif
  return std::max(kMinimumScrollableContentHeight, available_space);
}

void ProfileMenuViewBase::SetContentsView(views::View* view,
                                          int width_override) {
  RemoveAllChildViews(true);
  if (width_override == -1)
    width_override = menu_width_;
  views::GridLayout* layout = CreateSingleColumnLayout(this, width_override);

  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->set_hide_horizontal_scrollbar(true);

  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->set_draw_overflow_indicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(view);

  layout->StartRow(1.0, 0);
  layout->AddView(scroll_view);
  if (GetBubbleFrameView()) {
    SizeToContents();
    // SizeToContents() will perform a layout, but only if the size changed.
    Layout();
  }
}

views::GridLayout* ProfileMenuViewBase::CreateSingleColumnLayout(
    views::View* view,
    int width) {
  // TODO(https://crbug.com/934689):
  // DEPRECATED: New user menu components should use views::BoxLayout instead.
  // Creates a GridLayout with a single column. This ensures that all the child
  // views added get auto-expanded to fill the full width of the bubble.
  views::GridLayout* layout =
      view->SetLayoutManager(std::make_unique<views::GridLayout>(view));

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     width, width);
  return layout;
}
