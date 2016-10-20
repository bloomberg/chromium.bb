// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/three_view_layout.h"

#include "ash/common/system/tray/size_range_layout.h"
#include "base/logging.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace {

// Converts ThreeViewLayout::Orientation values to
// views::BoxLayout::Orientation values.
views::BoxLayout::Orientation GetOrientation(
    ThreeViewLayout::Orientation orientation) {
  switch (orientation) {
    case ThreeViewLayout::Orientation::HORIZONTAL:
      return views::BoxLayout::kHorizontal;
    case ThreeViewLayout::Orientation::VERTICAL:
      return views::BoxLayout::kVertical;
  }
  // Required for some compilers.
  NOTREACHED();
  return views::BoxLayout::kHorizontal;
}

}  // namespace

ThreeViewLayout::ThreeViewLayout() : ThreeViewLayout(0) {}

ThreeViewLayout::ThreeViewLayout(int padding_between_containers)
    : ThreeViewLayout(Orientation::HORIZONTAL, padding_between_containers) {}

ThreeViewLayout::ThreeViewLayout(Orientation orientation)
    : ThreeViewLayout(orientation, 0) {}

ThreeViewLayout::ThreeViewLayout(Orientation orientation,
                                 int padding_between_containers)
    : box_layout_(new views::BoxLayout(GetOrientation(orientation),
                                       0,
                                       0,
                                       padding_between_containers)),
      start_container_(new views::View),
      start_container_layout_manager_(new SizeRangeLayout),
      center_container_(new views::View),
      center_container_layout_manager_(new SizeRangeLayout),
      end_container_(new views::View),
      end_container_layout_manager_(new SizeRangeLayout) {
  start_container_->SetLayoutManager(GetLayoutManager(Container::START));
  center_container_->SetLayoutManager(GetLayoutManager(Container::CENTER));
  end_container_->SetLayoutManager(GetLayoutManager(Container::END));
  GetLayoutManager(Container::START)
      ->SetLayoutManager(CreateDefaultLayoutManager(orientation));
  GetLayoutManager(Container::CENTER)
      ->SetLayoutManager(CreateDefaultLayoutManager(orientation));
  GetLayoutManager(Container::END)
      ->SetLayoutManager(CreateDefaultLayoutManager(orientation));
}

ThreeViewLayout::~ThreeViewLayout() {
  if (host_) {
    host_->RemoveChildView(GetContainer(Container::START));
    host_->RemoveChildView(GetContainer(Container::CENTER));
    host_->RemoveChildView(GetContainer(Container::END));
  }
}

void ThreeViewLayout::SetMinCrossAxisSize(int min_size) {
  box_layout_->set_minimum_cross_axis_size(min_size);
}

void ThreeViewLayout::SetMinSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMinSize(size);
}

void ThreeViewLayout::SetMaxSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMaxSize(size);
}

void ThreeViewLayout::AddView(Container container, views::View* view) {
  GetContainer(container)->AddChildView(view);
}

void ThreeViewLayout::SetView(Container container, views::View* view) {
  views::View* container_view = GetContainer(container);
  container_view->RemoveAllChildViews(true);
  container_view->AddChildView(view);
}

void ThreeViewLayout::SetInsets(const gfx::Insets& insets) {
  box_layout_->set_inside_border_insets(insets);
}

void ThreeViewLayout::SetBorder(Container container,
                                std::unique_ptr<views::Border> border) {
  GetContainer(container)->SetBorder(std::move(border));
}

void ThreeViewLayout::SetContainerVisible(Container container, bool visible) {
  GetContainer(container)->SetVisible(visible);
}

void ThreeViewLayout::SetFlexForContainer(Container container, int flex) {
  box_layout_->SetFlexForView(GetContainer(container), flex);
}

void ThreeViewLayout::SetLayoutManager(
    Container container,
    std::unique_ptr<LayoutManager> layout_manager) {
  GetLayoutManager(container)->SetLayoutManager(std::move(layout_manager));
  if (GetLayoutManager(container))
    GetLayoutManager(container)->Installed(GetContainer(container));
}

void ThreeViewLayout::Installed(views::View* host) {
  DCHECK(!host_);
  host_ = host;
  if (host_) {
    host_->AddChildView(GetContainer(Container::START));
    host_->AddChildView(GetContainer(Container::CENTER));
    host_->AddChildView(GetContainer(Container::END));
  }
  box_layout_->Installed(host_);
}

void ThreeViewLayout::Layout(views::View* host) {
  box_layout_->Layout(host);
}

gfx::Size ThreeViewLayout::GetPreferredSize(const views::View* host) const {
  return box_layout_->GetPreferredSize(host);
}

int ThreeViewLayout::GetPreferredHeightForWidth(const views::View* host,
                                                int width) const {
  return box_layout_->GetPreferredHeightForWidth(host, width);
}

void ThreeViewLayout::ViewAdded(views::View* host, views::View* view) {
  box_layout_->ViewAdded(host, view);
}

void ThreeViewLayout::ViewRemoved(views::View* host, views::View* view) {
  box_layout_->ViewRemoved(host, view);
}

std::unique_ptr<views::LayoutManager>
ThreeViewLayout::CreateDefaultLayoutManager(Orientation orientation) const {
  views::BoxLayout* box_layout =
      new views::BoxLayout(GetOrientation(orientation), 0, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

views::View* ThreeViewLayout::GetContainer(Container container) const {
  switch (container) {
    case Container::START:
      return start_container_;
    case Container::CENTER:
      return center_container_;
    case Container::END:
      return end_container_;
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

SizeRangeLayout* ThreeViewLayout::GetLayoutManager(Container container) const {
  switch (container) {
    case Container::START:
      return start_container_layout_manager_;
    case Container::CENTER:
      return center_container_layout_manager_;
    case Container::END:
      return end_container_layout_manager_;
  }
  // Required for some compilers.
  NOTREACHED();
  return nullptr;
}

}  // namespace ash
