// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/side_tab_strip.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tabs/side_tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/background.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/text_button.h"

namespace {

const int kVerticalTabSpacing = 2;
const int kTabStripWidth = 140;
const SkColor kBackgroundColor = SkColorSetARGB(255, 209, 220, 248);
const SkColor kSeparatorColor = SkColorSetARGB(255, 151, 159, 179);

// Height of the scroll buttons.
const int kScrollButtonHeight = 20;

// Height of the separator.
const int kSeparatorHeight = 1;

// Padding between tabs and scroll button.
const int kScrollButtonVerticalPadding = 2;

// The new tab button is rendered using a SideTab.
class SideTabNewTabButton : public SideTab {
 public:
  explicit SideTabNewTabButton(TabStripController* controller);

  virtual bool ShouldPaintHighlight() const OVERRIDE { return false; }
  virtual bool IsSelected() const OVERRIDE { return false; }
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;

 private:
  TabStripController* controller_;

  DISALLOW_COPY_AND_ASSIGN(SideTabNewTabButton);
};

SideTabNewTabButton::SideTabNewTabButton(TabStripController* controller)
    : SideTab(NULL),
      controller_(controller) {
  // Never show a close button for the new tab button.
  close_button()->SetVisible(false);
  TabRendererData data;
  data.favicon = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_SIDETABS_NEW_TAB);
  data.title = l10n_util::GetStringUTF16(IDS_NEW_TAB_TITLE);
  SetData(data);
}

bool SideTabNewTabButton::OnMousePressed(const views::MouseEvent& event) {
  return true;
}

void SideTabNewTabButton::OnMouseReleased(const views::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && HitTest(event.location()))
    controller_->CreateNewTab();
}

// Button class used for the scroll buttons.
class ScrollButton : public views::TextButton {
 public:
  enum Type {
    UP,
    DOWN
  };

  explicit ScrollButton(views::ButtonListener* listener, Type type);

 protected:
  // views::View overrides.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  const Type type_;

  DISALLOW_COPY_AND_ASSIGN(ScrollButton);
};

ScrollButton::ScrollButton(views::ButtonListener* listener,
                           Type type)
    : views::TextButton(listener, std::wstring()),
      type_(type) {
}

void ScrollButton::OnPaint(gfx::Canvas* canvas) {
  TextButton::OnPaint(canvas);

  // Draw the arrow.
  SkColor arrow_color = IsEnabled() ? SK_ColorBLACK : SK_ColorGRAY;
  int arrow_height = 5;
  int x = width() / 2;
  int y = (height() - arrow_height) / 2;
  int delta_y = 1;
  if (type_ == DOWN) {
    delta_y = -1;
    y += arrow_height;
  }
  for (int i = 0; i < arrow_height; ++i, --x, y += delta_y)
    canvas->FillRectInt(arrow_color, x, y, (i * 2) + 1, 1);
}

}  // namespace

// static
const int SideTabStrip::kTabStripInset = 3;

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip(TabStripController* controller)
    : BaseTabStrip(controller, BaseTabStrip::VERTICAL_TAB_STRIP),
      newtab_button_(new SideTabNewTabButton(controller)),
      scroll_up_button_(NULL),
      scroll_down_button_(NULL),
      separator_(new views::View()),
      first_tab_y_offset_(0),
      ideal_height_(0) {
  SetID(VIEW_ID_TAB_STRIP);
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  AddChildView(newtab_button_);
  separator_->set_background(
      views::Background::CreateSolidBackground(kSeparatorColor));
  AddChildView(separator_);
  scroll_up_button_ = new ScrollButton(this, ScrollButton::UP);
  AddChildView(scroll_up_button_);
  scroll_down_button_ = new ScrollButton(this, ScrollButton::DOWN);
  AddChildView(scroll_down_button_);
}

SideTabStrip::~SideTabStrip() {
  DestroyDragController();
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, AbstractTabStripView implementation:

bool SideTabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  return GetEventHandlerForPoint(point) == this;
}

void SideTabStrip::SetBackgroundOffset(const gfx::Point& offset) {
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, BaseTabStrip implementation:

void SideTabStrip::StartHighlight(int model_index) {
}

void SideTabStrip::StopAllHighlighting() {
}

BaseTab* SideTabStrip::CreateTabForDragging() {
  SideTab* tab = new SideTab(NULL);
  // Make sure the dragged tab shares our theme provider. We need to explicitly
  // do this as during dragging there isn't a theme provider.
  tab->set_theme_provider(GetThemeProvider());
  return tab;
}

void SideTabStrip::RemoveTabAt(int model_index) {
  StartRemoveTabAnimation(model_index);
}

void SideTabStrip::SelectTabAt(int old_model_index, int new_model_index) {
  GetBaseTabAtModelIndex(new_model_index)->SchedulePaint();

  if (controller()->IsActiveTab(new_model_index))
    MakeTabVisible(ModelIndexToTabIndex(new_model_index));
}

void SideTabStrip::TabTitleChangedNotLoading(int model_index) {
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(kTabStripWidth, 0);
}

void SideTabStrip::PaintChildren(gfx::Canvas* canvas) {
  // Make sure any tabs being dragged appear on top of all others by painting
  // them last.
  std::vector<BaseTab*> dragging_tabs;

  // Make sure nothing draws on top of the scroll buttons.
  canvas->Save();
  canvas->ClipRectInt(kTabStripInset, kTabStripInset,
                      width() - kTabStripInset - kTabStripInset,
                      GetMaxTabY() - kTabStripInset);

  // Paint the new tab and separator first so that any tabs animating appear on
  // top.
  separator_->Paint(canvas);
  newtab_button_->Paint(canvas);

  for (int i = tab_count() - 1; i >= 0; --i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (tab->dragging())
      dragging_tabs.push_back(tab);
    else
      tab->Paint(canvas);
  }

  for (size_t i = 0; i < dragging_tabs.size(); ++i)
    dragging_tabs[i]->Paint(canvas);

  canvas->Restore();

  scroll_down_button_->Paint(canvas);
  scroll_up_button_->Paint(canvas);
}

views::View* SideTabStrip::GetEventHandlerForPoint(const gfx::Point& point) {
  // Check the scroll buttons first as they visually appear on top of everything
  // else.
  if (scroll_down_button_->IsVisible()) {
    gfx::Point local_point(point);
    View::ConvertPointToView(this, scroll_down_button_, &local_point);
    if (scroll_down_button_->HitTest(local_point))
      return scroll_down_button_->GetEventHandlerForPoint(local_point);
  }

  if (scroll_up_button_->IsVisible()) {
    gfx::Point local_point(point);
    View::ConvertPointToView(this, scroll_up_button_, &local_point);
    if (scroll_up_button_->HitTest(local_point))
      return scroll_up_button_->GetEventHandlerForPoint(local_point);
  }
  return views::View::GetEventHandlerForPoint(point);
}

void SideTabStrip::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
  int max_offset = GetMaxOffset();
  if (max_offset == 0) {
    // All the tabs fit, no need to scroll.
    return;
  }

  // Determine the index of the first visible tab.
  int initial_y = kTabStripInset;
  int first_vis_index = -1;
  for (int i = 0; i < tab_count(); ++i) {
    if (ideal_bounds(i).bottom() > initial_y) {
      first_vis_index = i;
      break;
    }
  }
  if (first_vis_index == -1)
    return;

  int delta = 0;
  if (sender == scroll_up_button_) {
    delta = initial_y - ideal_bounds(first_vis_index).y();
    if (delta <= 0) {
      if (first_vis_index == 0) {
        delta = -first_tab_y_offset_;
      } else {
        delta = initial_y - ideal_bounds(first_vis_index - 1).y();
        DCHECK_NE(0, delta);  // Not fatal, but indicates we aren't scrolling.
      }
    }
  } else {
    DCHECK_EQ(sender, scroll_down_button_);
    if (ideal_bounds(first_vis_index).y() > initial_y) {
      delta = initial_y - ideal_bounds(first_vis_index).y();
    } else if (first_vis_index + 1 == tab_count()) {
      delta = -first_tab_y_offset_;
    } else {
      delta = initial_y - ideal_bounds(first_vis_index + 1).y();
    }
  }
  SetFirstTabYOffset(first_tab_y_offset_ + delta);
}

BaseTab* SideTabStrip::CreateTab() {
  return new SideTab(this);
}

void SideTabStrip::GenerateIdealBounds() {
  gfx::Rect layout_rect = GetContentsBounds();
  layout_rect.Inset(kTabStripInset, kTabStripInset);

  int y = layout_rect.y() + first_tab_y_offset_;
  bool last_was_mini = true;
  bool has_non_closing_tab = false;
  separator_bounds_.SetRect(0, -kSeparatorHeight, width(), kSeparatorHeight);
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (!tab->closing()) {
      if (last_was_mini != tab->data().mini) {
        if (has_non_closing_tab) {
          separator_bounds_.SetRect(0, y, width(), kSeparatorHeight);
          y += kSeparatorHeight + kVerticalTabSpacing;
        }
        newtab_button_bounds_.SetRect(
            layout_rect.x(), y, layout_rect.width(),
            newtab_button_->GetPreferredSize().height());
        y = newtab_button_bounds_.bottom() + kVerticalTabSpacing;
        last_was_mini = tab->data().mini;
      }
      gfx::Rect bounds = gfx::Rect(layout_rect.x(), y, layout_rect.width(),
                                   tab->GetPreferredSize().height());
      set_ideal_bounds(i, bounds);
      y = bounds.bottom() + kVerticalTabSpacing;
      has_non_closing_tab = true;
    }
  }

  if (last_was_mini) {
    if (has_non_closing_tab) {
      separator_bounds_.SetRect(0, y, width(), kSeparatorHeight);
      y += kSeparatorHeight + kVerticalTabSpacing;
    }
    newtab_button_bounds_ =
        gfx::Rect(layout_rect.x(), y, layout_rect.width(),
                  newtab_button_->GetPreferredSize().height());
    y += newtab_button_->GetPreferredSize().height();
  }

  ideal_height_ = y - layout_rect.y() - first_tab_y_offset_;

  scroll_up_button_->SetEnabled(first_tab_y_offset_ != 0);
  scroll_down_button_->SetEnabled(GetMaxOffset() != 0 &&
                                  first_tab_y_offset_ != GetMaxOffset());
}

void SideTabStrip::StartInsertTabAnimation(int model_index) {
  PrepareForAnimation();

  GenerateIdealBounds();

  int tab_data_index = ModelIndexToTabIndex(model_index);
  BaseTab* tab = base_tab_at_tab_index(tab_data_index);
  if (model_index == 0) {
    tab->SetBounds(ideal_bounds(tab_data_index).x(), 0,
                   ideal_bounds(tab_data_index).width(), 0);
  } else {
    BaseTab* last_tab = base_tab_at_tab_index(tab_data_index - 1);
    tab->SetBounds(last_tab->x(), last_tab->bounds().bottom(),
                   ideal_bounds(tab_data_index).width(), 0);
  }

  AnimateToIdealBounds();
}

void SideTabStrip::AnimateToIdealBounds() {
  for (int i = 0; i < tab_count(); ++i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (!tab->closing() && !tab->dragging())
      bounds_animator().AnimateViewTo(tab, ideal_bounds(i));
  }

  bounds_animator().AnimateViewTo(newtab_button_, newtab_button_bounds_);

  bounds_animator().AnimateViewTo(separator_, separator_bounds_);
}

void SideTabStrip::DoLayout() {
  BaseTabStrip::DoLayout();
  newtab_button_->SetBoundsRect(newtab_button_bounds_);
  separator_->SetBoundsRect(separator_bounds_);
  int scroll_button_y = height() - kScrollButtonHeight;
  scroll_up_button_->SetBounds(0, scroll_button_y, width() / 2,
                               kScrollButtonHeight);
  scroll_down_button_->SetBounds(width() / 2, scroll_button_y, width() / 2,
                                 kScrollButtonHeight);
}

void SideTabStrip::LayoutDraggedTabsAt(const std::vector<BaseTab*>& tabs,
                                       BaseTab* active_tab,
                                       const gfx::Point& location,
                                       bool initial_drag) {
  // TODO: add support for initial_drag (see TabStrip's implementation).
  gfx::Rect layout_rect = GetContentsBounds();
  layout_rect.Inset(kTabStripInset, kTabStripInset);
  int y = location.y();
  for (size_t i = 0; i < tabs.size(); ++i) {
    BaseTab* tab = tabs[i];
    tab->SchedulePaint();
    tab->SetBounds(layout_rect.x(), y, layout_rect.width(),
                   tab->GetPreferredSize().height());
    tab->SchedulePaint();
    y += tab->height() + kVerticalTabSpacing;
  }
}

void SideTabStrip::CalculateBoundsForDraggedTabs(
    const std::vector<BaseTab*>& tabs,
    std::vector<gfx::Rect>* bounds) {
  int y = 0;
  for (size_t i = 0; i < tabs.size(); ++i) {
    BaseTab* tab = tabs[i];
    gfx::Rect tab_bounds(tab->bounds());
    tab_bounds.set_y(y);
    y += tab->height() + kVerticalTabSpacing;
    bounds->push_back(tab_bounds);
  }
}

int SideTabStrip::GetSizeNeededForTabs(const std::vector<BaseTab*>& tabs) {
  return static_cast<int>(tabs.size()) * SideTab::GetPreferredHeight();
}

void SideTabStrip::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // When our height changes we may be able to show more.
  first_tab_y_offset_ = std::max(GetMaxOffset(),
                                 std::min(0, first_tab_y_offset_));
  for (int i = 0; i < controller()->GetCount(); ++i) {
    if (controller()->IsActiveTab(i)) {
      MakeTabVisible(ModelIndexToTabIndex(i));
      break;
    }
  }
}

void SideTabStrip::SetFirstTabYOffset(int new_offset) {
  int max_offset = GetMaxOffset();
  if (max_offset == 0) {
    // All the tabs fit, no need to scroll.
    return;
  }
  new_offset = std::max(max_offset, std::min(0, new_offset));
  if (new_offset == first_tab_y_offset_)
    return;

  StopAnimating(false);
  first_tab_y_offset_ = new_offset;
  GenerateIdealBounds();
  DoLayout();

}

int SideTabStrip::GetMaxOffset() const {
  int available_height = GetMaxTabY() - kTabStripInset;
  return std::min(0, available_height - ideal_height_);
}

int SideTabStrip::GetMaxTabY() const {
  return height() - kTabStripInset - kScrollButtonVerticalPadding -
      kScrollButtonHeight;
}

void SideTabStrip::MakeTabVisible(int tab_index) {
  if (height() == 0)
    return;

  if (ideal_bounds(tab_index).y() < kTabStripInset) {
    SetFirstTabYOffset(first_tab_y_offset_ - ideal_bounds(tab_index).y() +
                       kTabStripInset);
  } else if (ideal_bounds(tab_index).bottom() > GetMaxTabY()) {
    SetFirstTabYOffset(GetMaxTabY() - (ideal_bounds(tab_index).bottom() -
                                       first_tab_y_offset_));
  }
}
