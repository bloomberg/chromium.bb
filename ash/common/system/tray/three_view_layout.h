// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_THREE_VIEW_LAYOUT_H_
#define ASH_COMMON_SYSTEM_TRAY_THREE_VIEW_LAYOUT_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_manager.h"

namespace views {
class Border;
class BoxLayout;
class View;
}  // namespace views

namespace ash {
class SizeRangeLayout;

// A layout manager that has 3 child containers (START, CENTER, END) which can
// be arranged vertically or horizontally. The child containers can have
// minimum and/or maximum preferred size defined as well as a flex weight that
// is used to distribute excess space across the main axis, i.e. flexible width
// for the horizontal orientation. By default all the containers have a flex
// weight of 0, meaning no flexibility, and no minimum or maximum size.
//
// Views are laid out within each container as per the LayoutManager that has
// been installed on that container. By default a BoxLayout manager is installed
// on each container with the same orientation as |this| has been created with.
// The default BoxLayout will use a center alignment for both the main axis and
// cross axis alignment.
class ASH_EXPORT ThreeViewLayout : public views::LayoutManager {
 public:
  enum class Orientation {
    HORIZONTAL,
    VERTICAL,
  };

  // The different containers that child Views can be added to.
  enum class Container { START, CENTER, END };

  // Constructs a layout with horizontal orientation and 0 padding between
  // containers.
  ThreeViewLayout();

  // Constructs a layout with Horizontal orientation and the specified padding
  // between containers.
  //
  // TODO(bruthig): The |padding_between_containers| can only be set on
  // BoxLayouts during construction. Investigate whether this can be a mutable
  // property of BoxLayouts and if so consider dropping it as a constructor
  // parameter here.
  explicit ThreeViewLayout(int padding_between_containers);

  // Constructs a layout with the specified orientation and 0 padding between
  // containers.
  explicit ThreeViewLayout(Orientation orientation);

  // Constructs a layout with the specified orientation and padding between
  // containers.
  ThreeViewLayout(Orientation orientation, int padding_between_containers);

  ~ThreeViewLayout() override;

  // Set the minimum cross axis size for the layout, i.e. the minimum height for
  // a horizontal orientation.
  void SetMinCrossAxisSize(int min_size);

  // Set the minimum size for the given |container|.
  void SetMinSize(Container container, const gfx::Size& size);

  // Set the maximum size for the given |container|.
  void SetMaxSize(Container container, const gfx::Size& size);

  // Adds the child |view| to the specified |container|.
  void AddView(Container container, views::View* view);

  // Removes all current children from the specified |container| and adds the
  // child |view| to it.
  void SetView(Container container, views::View* view);

  // During layout the |insets| are applied to the host views entire space
  // before allocating the remaining space to the container views.
  void SetInsets(const gfx::Insets& insets);

  // Sets the border for the given |container|.
  void SetBorder(Container container, std::unique_ptr<views::Border> border);

  // Sets wither the |container| is visible. During a layout the space will be
  // allocated to the visible containers only. i.e. non-visible containers will
  // not be allocated any space.
  void SetContainerVisible(Container container, bool visible);

  // Sets the flex weight for the given |container|. Using the preferred size as
  // the basis, free space along the main axis is distributed to views in the
  // ratio of their flex weights. Similarly, if the views will overflow the
  // parent, space is subtracted in these ratios.
  //
  // A flex of 0 means this view is not resized. Flex values must not be
  // negative.
  //
  // Note that non-zero flex values will take precedence over size constraints.
  // i.e. even if |container| has a max size set the space allocated during
  // layout may be larger if |flex| > 0 and similar for min size constraints.
  void SetFlexForContainer(Container container, int flex);

  // Sets the |layout_manager| used by the given |container|.
  void SetLayoutManager(Container container,
                        std::unique_ptr<LayoutManager> layout_manager);

  // LayoutManager:
  void Installed(views::View* host) override;
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
  int GetPreferredHeightForWidth(const views::View* host,
                                 int width) const override;
  void ViewAdded(views::View* host, views::View* view) override;
  void ViewRemoved(views::View* host, views::View* view) override;

 private:
  friend class ThreeViewLayoutTest;

  // Creates a default LayoutManager for the given |orientation|.
  std::unique_ptr<views::LayoutManager> CreateDefaultLayoutManager(
      Orientation orientation) const;

  // Returns the View for the given |container|.
  views::View* GetContainer(Container container) const;

  // Returns the layout manager for the given |container|.
  SizeRangeLayout* GetLayoutManager(Container container) const;

  // The layout manager that lays out the different Container Views.
  std::unique_ptr<views::BoxLayout> box_layout_;

  // The host View that this layout manager has been installed on.
  views::View* host_ = nullptr;

  views::View* start_container_ = nullptr;
  SizeRangeLayout* start_container_layout_manager_ = nullptr;

  views::View* center_container_ = nullptr;
  SizeRangeLayout* center_container_layout_manager_ = nullptr;

  views::View* end_container_ = nullptr;
  SizeRangeLayout* end_container_layout_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ThreeViewLayout);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_THREE_VIEW_LAYOUT_H_
