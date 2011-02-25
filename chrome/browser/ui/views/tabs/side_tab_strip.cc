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

namespace {
const int kVerticalTabSpacing = 2;
const int kTabStripWidth = 140;
const SkColor kBackgroundColor = SkColorSetARGB(255, 209, 220, 248);
const SkColor kSeparatorColor = SkColorSetARGB(255, 151, 159, 179);

// Height of the separator.
const int kSeparatorHeight = 1;

// The new tab button is rendered using a SideTab.
class SideTabNewTabButton : public SideTab {
 public:
  explicit SideTabNewTabButton(TabStripController* controller);

  virtual bool ShouldPaintHighlight() const { return false; }
  virtual bool IsSelected() const { return false; }
  bool OnMousePressed(const views::MouseEvent& event);
  void OnMouseReleased(const views::MouseEvent& event, bool canceled);

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

void SideTabNewTabButton::OnMouseReleased(const views::MouseEvent& event,
                                          bool canceled) {
  if (!canceled && event.IsOnlyLeftMouseButton() && HitTest(event.location()))
    controller_->CreateNewTab();
}

}  // namespace

// static
const int SideTabStrip::kTabStripInset = 3;

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, public:

SideTabStrip::SideTabStrip(TabStripController* controller)
    : BaseTabStrip(controller, BaseTabStrip::VERTICAL_TAB_STRIP),
      newtab_button_(new SideTabNewTabButton(controller)),
      separator_(new views::View()) {
  SetID(VIEW_ID_TAB_STRIP);
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
  AddChildView(newtab_button_);
  separator_->set_background(
      views::Background::CreateSolidBackground(kSeparatorColor));
  AddChildView(separator_);
}

SideTabStrip::~SideTabStrip() {
  DestroyDragController();
}

////////////////////////////////////////////////////////////////////////////////
// SideTabStrip, BaseTabStrip implementation:

void SideTabStrip::SetBackgroundOffset(const gfx::Point& offset) {
}

bool SideTabStrip::IsPositionInWindowCaption(const gfx::Point& point) {
  return GetEventHandlerForPoint(point) == this;
}

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
}

void SideTabStrip::TabTitleChangedNotLoading(int model_index) {
}

gfx::Size SideTabStrip::GetPreferredSize() {
  return gfx::Size(kTabStripWidth, 0);
}

void SideTabStrip::PaintChildren(gfx::Canvas* canvas) {
  // Make sure the dragged tab appears on top of all others by paint it last.
  BaseTab* dragging_tab = NULL;

  // Paint the new tab and separator first so that any tabs animating appear on
  // top.
  separator_->Paint(canvas);
  newtab_button_->Paint(canvas);

  for (int i = tab_count() - 1; i >= 0; --i) {
    BaseTab* tab = base_tab_at_tab_index(i);
    if (tab->dragging())
      dragging_tab = tab;
    else
      tab->Paint(canvas);
  }

  if (dragging_tab)
    dragging_tab->Paint(canvas);
}

BaseTab* SideTabStrip::CreateTab() {
  return new SideTab(this);
}

void SideTabStrip::GenerateIdealBounds() {
  gfx::Rect layout_rect = GetContentsBounds();
  layout_rect.Inset(kTabStripInset, kTabStripInset);

  int y = layout_rect.y();
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
  }
}

void SideTabStrip::StartInsertTabAnimation(int model_index, bool foreground) {
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
}
