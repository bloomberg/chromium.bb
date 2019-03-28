// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_menu_view_base.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

namespace {

ProfileMenuViewBase* g_profile_bubble_ = nullptr;

// Helpers --------------------------------------------------------------------

// If the bubble is too large to fit on the screen, it still needs to be at
// least this tall to show one row.
constexpr int kMinimumScrollableContentHeight = 40;

}  // namespace

// ProfileMenuViewBase ---------------------------------------------------------

// static
bool ProfileMenuViewBase::IsShowing() {
  return g_profile_bubble_ != nullptr;
}

// static
void ProfileMenuViewBase::Hide() {
  if (g_profile_bubble_)
    g_profile_bubble_->GetWidget()->Close();
}

// static
ProfileMenuViewBase* ProfileMenuViewBase::GetBubbleForTesting() {
  return g_profile_bubble_;
}

ProfileMenuViewBase::ProfileMenuViewBase(views::Button* anchor_button,
                                         const gfx::Rect& anchor_rect,
                                         gfx::NativeView parent_window,
                                         Browser* browser)
    : BubbleDialogDelegateView(anchor_button, views::BubbleBorder::TOP_RIGHT),
      browser_(browser),
      menu_width_(0),
      anchor_button_(anchor_button),
      close_bubble_helper_(this, browser) {
  DCHECK(!g_profile_bubble_);
  g_profile_bubble_ = this;
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

ProfileMenuViewBase::~ProfileMenuViewBase() {
  // Items stored for menu generation are removed after menu is finalized, hence
  // it's not expected to have while destroying the object.
  DCHECK(g_profile_bubble_ != this);
  DCHECK(menu_item_groups_.empty());
}

ax::mojom::Role ProfileMenuViewBase::GetAccessibleWindowRole() const {
  // Return |ax::mojom::Role::kDialog| which will make screen readers announce
  // the following in the listed order:
  // the title of the dialog, labels (if any), the focused View within the
  // dialog (if any)
  return ax::mojom::Role::kDialog;
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

void ProfileMenuViewBase::WindowClosing() {
  DCHECK_EQ(g_profile_bubble_, this);
  if (anchor_button())
    anchor_button()->AnimateInkDrop(views::InkDropState::DEACTIVATED, nullptr);
  g_profile_bubble_ = nullptr;
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

void ProfileMenuViewBase::ResetMenu() {
  menu_item_groups_.clear();
}

void ProfileMenuViewBase::AddMenuItems(MenuItems& menu_items, bool new_group) {
  if (new_group || menu_item_groups_.empty())
    menu_item_groups_.emplace_back();

  auto& last_group = menu_item_groups_.back();
  for (auto& item : menu_items)
    last_group.push_back(std::move(item));
}

void ProfileMenuViewBase::RepopulateViewFromMenuItems() {
  RemoveAllChildViews(true);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  int border_insets =
      provider->GetDistanceMetric(DISTANCE_CONTENT_LIST_VERTICAL_SINGLE);

  std::unique_ptr<views::View> main_view = std::make_unique<views::View>();
  main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(border_insets, 0),
      border_insets));

  for (size_t i = 0; i < menu_item_groups_.size(); i++) {
    if (i == 0)
      main_view->AddChildView(new views::Separator());

    views::View* sub_view = new views::View();
    sub_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::kVertical, gfx::Insets(i ? border_insets : 0, 0)));

    for (std::unique_ptr<views::View>& item : menu_item_groups_[i])
      sub_view->AddChildView(std::move(item));

    main_view->AddChildView(sub_view);
  }

  menu_item_groups_.clear();

  // TODO(https://crbug.com/934689): Simplify. This part is only done to set
  // the menu width.
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     views::GridLayout::kFixedSize, views::GridLayout::FIXED,
                     menu_width_, menu_width_);

  views::ScrollView* scroll_view = new views::ScrollView;
  scroll_view->set_hide_horizontal_scrollbar(true);

  // TODO(https://crbug.com/871762): it's a workaround for the crash.
  scroll_view->set_draw_overflow_indicator(false);
  scroll_view->ClipHeightTo(0, GetMaxHeight());
  scroll_view->SetContents(std::move(main_view));

  layout->StartRow(1.0, 0);
  layout->AddView(scroll_view);
  if (GetBubbleFrameView()) {
    SizeToContents();
    // SizeToContents() will perform a layout, but only if the size changed.
    Layout();
  }
}
