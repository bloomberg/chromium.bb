// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tri_view.h"

#include "ash/common/system/tray/size_range_layout.h"
#include "base/logging.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"

namespace ash {
namespace {

// Converts TriView::Orientation values to views::BoxLayout::Orientation values.
views::BoxLayout::Orientation GetOrientation(TriView::Orientation orientation) {
  switch (orientation) {
    case TriView::Orientation::HORIZONTAL:
      return views::BoxLayout::kHorizontal;
    case TriView::Orientation::VERTICAL:
      return views::BoxLayout::kVertical;
  }
  // Required for some compilers.
  NOTREACHED();
  return views::BoxLayout::kHorizontal;
}

// A View that will perform a layout if a child view's preferred size changes.
class RelayoutView : public views::View {
 public:
  RelayoutView() {}

  // views::View:
  void ChildPreferredSizeChanged(View* child) override { Layout(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(RelayoutView);
};

}  // namespace

TriView::TriView() : TriView(0) {}

TriView::TriView(int padding_between_containers)
    : TriView(Orientation::HORIZONTAL, padding_between_containers) {}

TriView::TriView(Orientation orientation) : TriView(orientation, 0) {}

TriView::TriView(Orientation orientation, int padding_between_containers)
    : box_layout_(new views::BoxLayout(GetOrientation(orientation),
                                       0,
                                       0,
                                       padding_between_containers)),
      start_container_layout_manager_(new SizeRangeLayout),
      center_container_layout_manager_(new SizeRangeLayout),
      end_container_layout_manager_(new SizeRangeLayout) {
  AddChildView(new RelayoutView);
  AddChildView(new RelayoutView);
  AddChildView(new RelayoutView);

  GetContainer(Container::START)
      ->SetLayoutManager(GetLayoutManager(Container::START));
  GetContainer(Container::CENTER)
      ->SetLayoutManager(GetLayoutManager(Container::CENTER));
  GetContainer(Container::END)
      ->SetLayoutManager(GetLayoutManager(Container::END));

  box_layout_->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  SetLayoutManager(box_layout_);

  enable_hierarchy_changed_dcheck_ = true;
}

TriView::~TriView() {
  enable_hierarchy_changed_dcheck_ = false;
}

void TriView::SetMinSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMinSize(size);
}

gfx::Size TriView::GetMinSize(Container container) {
  return GetLayoutManager(container)->min_size();
}

void TriView::SetMaxSize(Container container, const gfx::Size& size) {
  GetLayoutManager(container)->SetMaxSize(size);
}

void TriView::AddView(Container container, views::View* view) {
  GetContainer(container)->AddChildView(view);
}

void TriView::RemoveAllChildren(Container container, bool delete_children) {
  GetContainer(container)->RemoveAllChildViews(delete_children);
}

void TriView::SetInsets(const gfx::Insets& insets) {
  box_layout_->set_inside_border_insets(insets);
}

void TriView::SetContainerBorder(Container container,
                                 std::unique_ptr<views::Border> border) {
  GetContainer(container)->SetBorder(std::move(border));
}

void TriView::SetContainerVisible(Container container, bool visible) {
  GetContainer(container)->SetVisible(visible);
}

void TriView::SetFlexForContainer(Container container, int flex) {
  box_layout_->SetFlexForView(GetContainer(container), flex);
}

void TriView::SetContainerLayout(
    Container container,
    std::unique_ptr<views::LayoutManager> layout_manager) {
  GetLayoutManager(container)->SetLayoutManager(std::move(layout_manager));
}

void TriView::ViewHierarchyChanged(
    const views::View::ViewHierarchyChangedDetails& details) {
  views::View::ViewHierarchyChanged(details);
  if (!enable_hierarchy_changed_dcheck_)
    return;

  if (details.parent == this) {
    if (details.is_add) {
      DCHECK(false)
          << "Child views should not be added directly. They should be added "
             "using TriView::AddView().";
    } else {
      DCHECK(false) << "Container views should not be removed.";
    }
  }
}

views::View* TriView::GetContainer(Container container) {
  return child_at(static_cast<int>(container));
}

SizeRangeLayout* TriView::GetLayoutManager(Container container) {
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
