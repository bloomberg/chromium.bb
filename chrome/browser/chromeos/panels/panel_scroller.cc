// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/panels/panel_scroller.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/panels/panel_scroller_container.h"
#include "chrome/browser/chromeos/panels/panel_scroller_header.h"
#include "ui/gfx/canvas.h"
#include "views/widget/widget.h"

struct PanelScroller::Panel {
  PanelScrollerHeader* header;
  PanelScrollerContainer* container;
};

PanelScroller::PanelScroller()
    : views::View(),
      divider_height_(18),
      needs_layout_(true),
      scroll_pos_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)),
      animated_scroll_begin_(0),
      animated_scroll_end_(0) {
  animation_.SetTweenType(ui::Tween::EASE_IN_OUT);
  animation_.SetSlideDuration(300);

  Panel* panel = new Panel;
  panel->header = new PanelScrollerHeader(this);
  panel->header->set_title(ASCIIToUTF16("Email"));
  panel->container = new PanelScrollerContainer(this, new views::View());
  panels_.push_back(panel);

  panel = new Panel;
  panel->header = new PanelScrollerHeader(this);
  panel->header->set_title(ASCIIToUTF16("Chat"));
  panel->container = new PanelScrollerContainer(this, new views::View());
  panels_.push_back(panel);

  panel = new Panel;
  panel->header = new PanelScrollerHeader(this);
  panel->header->set_title(ASCIIToUTF16("Calendar"));
  panel->container = new PanelScrollerContainer(this, new views::View());
  panels_.push_back(panel);

  panel = new Panel;
  panel->header = new PanelScrollerHeader(this);
  panel->header->set_title(ASCIIToUTF16("Recent searches"));
  panel->container = new PanelScrollerContainer(this, new views::View());
  panels_.push_back(panel);

  panel = new Panel;
  panel->header = new PanelScrollerHeader(this);
  panel->header->set_title(ASCIIToUTF16("Pony news"));
  panel->container = new PanelScrollerContainer(this, new views::View());
  panels_.push_back(panel);

  // Add the containers first since they're on the bottom.
  AddChildView(panels_[0]->container);
  AddChildView(panels_[1]->container);
  AddChildView(panels_[2]->container);
  AddChildView(panels_[3]->container);
  AddChildView(panels_[4]->container);

  AddChildView(panels_[0]->header);
  AddChildView(panels_[1]->header);
  AddChildView(panels_[2]->header);
  AddChildView(panels_[3]->header);
  AddChildView(panels_[4]->header);
}

PanelScroller::~PanelScroller() {
  STLDeleteContainerPointers(panels_.begin(), panels_.end());
}

// static
PanelScroller* PanelScroller::CreateWindow() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(0, 0, 100, 800);
  widget->Init(params);

  PanelScroller* scroller = new PanelScroller();
  widget->SetContentsView(scroller);

  widget->Show();

  return scroller;
}

void PanelScroller::ViewHierarchyChanged(bool is_add,
                                         views::View* parent,
                                         views::View* child) {
  // Our child views changed without us knowing it. Stop the animation and mark
  // us as dirty (needs_layout_ = true).
  animation_.Stop();
  needs_layout_ = true;
}

gfx::Size PanelScroller::GetPreferredSize() {
  return gfx::Size(75, 200);
}

void PanelScroller::Layout() {
/* TODO(brettw) this doesn't work for some reason.
  if (!needs_layout_ || !animation_.IsShowing())
    return;
  needs_layout_ = false;*/

  // The current location in the content that we're laying out. This is before
  // scrolling is accounted for.
  int cur_content_pos = 0;

  // Number of pixels used by headers stuck to the top of the scroll area.
  int top_header_pixel_count = 0;

  int panel_count = static_cast<int>(panels_.size());
  for (int i = 0; i < panel_count; i++) {
    if (cur_content_pos < scroll_pos_ + top_header_pixel_count) {
      // This panel is at least partially off the top. Put the header below the
      // others already there.
      panels_[i]->header->SetBoundsRect(gfx::Rect(0, top_header_pixel_count,
                                                  width(), divider_height_));
      top_header_pixel_count += divider_height_;

    } else if (cur_content_pos > height() + scroll_pos_ -
               (panel_count - i) * divider_height_) {
      // When we've hit the bottom of the visible content, all the remaining
      // headers will stack up at the bottom. Counting this header, there are
      // (size() - i) left, which is used in the expression above.
      int top = height() - (panel_count - i) * divider_height_;
      panels_[i]->header->SetBoundsRect(gfx::Rect(0, top,
                                                  width(), divider_height_));
    } else {
      // Normal header positioning in-flow.
      panels_[i]->header->SetBoundsRect(
          gfx::Rect(0, cur_content_pos - scroll_pos_, width(),
                    divider_height_));
    }

    cur_content_pos += divider_height_;

    // Now position the content. It always goes in-flow ignoring any stacked
    // up headers at the top or bottom.
    int container_height = panels_[i]->container->GetPreferredSize().height();
    panels_[i]->container->SetBoundsRect(
        gfx::Rect(0, cur_content_pos - scroll_pos_,
                  width(), container_height));
    cur_content_pos += container_height;
  }
}

bool PanelScroller::OnMousePressed(const views::MouseEvent& event) {
  return true;
}

bool PanelScroller::OnMouseDragged(const views::MouseEvent& event) {
  return true;
}

void PanelScroller::HeaderClicked(PanelScrollerHeader* source) {
  for (size_t i = 0; i < panels_.size(); i++) {
    if (panels_[i]->header == source) {
      ScrollToPanel(static_cast<int>(i));
      return;
    }
  }
  NOTREACHED() << "Invalid panel passed to HeaderClicked.";
}

void PanelScroller::ScrollToPanel(int index) {
  int affected_panel_height =
      panels_[index]->container->GetPreferredSize().height();

  // The pixel size we need to reserve for the stuck headers.
  int top_stuck_header_pixel_size = index * divider_height_;
  int bottom_stuck_header_pixel_size =
      (static_cast<int>(panels_.size()) - index - 1) * divider_height_;

  // Compute the offset of the top of the panel to scroll to.
  int space_above = 0;
  for (int i = 0; i < index; i++) {
    space_above += divider_height_ +
        panels_[i]->container->GetPreferredSize().height();
  }

  // Compute the space below the top of the panel.
  int space_below = 0;
  for (int i = index; i < static_cast<int>(panels_.size()); i++) {
    space_below += divider_height_ +
        panels_[i]->container->GetPreferredSize().height();
  }

  // The scroll position of the top of the stuck headers is the space above
  // minus the size of the headers stuck there.
  int top_stuck_headers_scroll_pos = space_above - top_stuck_header_pixel_size;

  // If the panel is already fully visible, do nothing.
  if (scroll_pos_ <= top_stuck_headers_scroll_pos &&
      space_above + divider_height_ + affected_panel_height -
      bottom_stuck_header_pixel_size <= scroll_pos_ + height())
    return;

  // Compute the scroll position.
  if (height() > space_below) {
    // There's enough room for this panel and everything below it to fit on the
    // screen.
    animated_scroll_end_ = (space_above + space_below) - height();
    if (animated_scroll_end_ > top_stuck_headers_scroll_pos)
      animated_scroll_end_ = top_stuck_headers_scroll_pos;
  } else if (space_above > scroll_pos_) {
    // If we're going to be scrolling the content up, scroll just until the
    // panel in question is fully visible.
    animated_scroll_end_ = space_above +
        divider_height_ + affected_panel_height +  // Size of this panel.
        bottom_stuck_header_pixel_size -  // Leave room for these.
        height();  // Available size in the window.
    if (animated_scroll_end_ > top_stuck_headers_scroll_pos)
      animated_scroll_end_ = top_stuck_headers_scroll_pos;
  } else {
    animated_scroll_end_ = top_stuck_headers_scroll_pos;
  }

  animated_scroll_begin_ = scroll_pos_;
  if (animated_scroll_begin_ == animated_scroll_end_)
    return;  // Nothing to animate.

  // Start animating to the destination.
  animation_.Reset();
  animation_.Show();
}

void PanelScroller::AnimationProgressed(const ui::Animation* animation) {
  scroll_pos_ = static_cast<int>(
      static_cast<double>(animated_scroll_end_ - animated_scroll_begin_) *
      animation_.GetCurrentValue()) + animated_scroll_begin_;

  Layout();
  SchedulePaint();
}
