// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/ca_layer_partial_damage_tree_mac.h"

#include "base/command_line.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/transform.h"

@interface CALayer(Private)
-(void)setContentsChanged;
@end

namespace content {
namespace {

// When selecting a CALayer to re-use for partial damage, this is the maximum
// fraction of the merged layer's pixels that may be not-updated by the swap
// before we consider the CALayer to not be a good enough match, and create a
// new one.
const float kMaximumPartialDamageWasteFraction = 1.2f;

// The maximum number of partial damage layers that may be created before we
// give up and remove them all (doing full damage in the process).
const size_t kMaximumPartialDamageLayers = 8;

}  // namespace

class CALayerPartialDamageTree::OverlayPlane {
 public:
  static linked_ptr<OverlayPlane> CreateWithFrameRect(
      int z_order,
      base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
      const gfx::RectF& pixel_frame_rect,
      const gfx::RectF& contents_rect) {
    gfx::Transform transform;
    transform.Translate(pixel_frame_rect.x(), pixel_frame_rect.y());
    return linked_ptr<OverlayPlane>(
        new OverlayPlane(z_order, io_surface, contents_rect, pixel_frame_rect));
  }

  ~OverlayPlane() {
    [ca_layer setContents:nil];
    [ca_layer removeFromSuperlayer];
    ca_layer.reset();
  }

  const int z_order;
  const base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
  const gfx::RectF contents_rect;
  const gfx::RectF pixel_frame_rect;
  bool layer_needs_update;
  base::scoped_nsobject<CALayer> ca_layer;

  void TakeCALayerFrom(OverlayPlane* other_plane) {
    ca_layer.swap(other_plane->ca_layer);
  }

  void UpdateProperties(float scale_factor) {
    if (layer_needs_update) {
      [ca_layer setOpaque:YES];

      id new_contents = static_cast<id>(io_surface.get());
      if ([ca_layer contents] == new_contents && z_order == 0)
        [ca_layer setContentsChanged];
      else
        [ca_layer setContents:new_contents];
      [ca_layer setContentsRect:contents_rect.ToCGRect()];

      [ca_layer setAnchorPoint:CGPointZero];

      if ([ca_layer respondsToSelector:(@selector(setContentsScale:))])
        [ca_layer setContentsScale:scale_factor];
      gfx::RectF dip_frame_rect = gfx::RectF(pixel_frame_rect);
      dip_frame_rect.Scale(1 / scale_factor);
      [ca_layer setBounds:CGRectMake(0, 0, dip_frame_rect.width(),
                                     dip_frame_rect.height())];
      [ca_layer
          setPosition:CGPointMake(dip_frame_rect.x(), dip_frame_rect.y())];
    }
    static bool show_borders =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kShowMacOverlayBorders);
    if (show_borders) {
      base::ScopedCFTypeRef<CGColorRef> color;
      if (!layer_needs_update) {
        // Green represents contents that are unchanged across frames.
        color.reset(CGColorCreateGenericRGB(0, 1, 0, 1));
      } else {
        // Red represents damaged contents.
        color.reset(CGColorCreateGenericRGB(1, 0, 0, 1));
      }
      [ca_layer setBorderWidth:1];
      [ca_layer setBorderColor:color];
    }
    layer_needs_update = false;
  }

 private:
  OverlayPlane(int z_order,
               base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
               const gfx::RectF& contents_rect,
               const gfx::RectF& pixel_frame_rect)
      : z_order(z_order),
        io_surface(io_surface),
        contents_rect(contents_rect),
        pixel_frame_rect(pixel_frame_rect),
        layer_needs_update(true) {}
};

void CALayerPartialDamageTree::UpdateRootAndPartialDamagePlanes(
    CALayerPartialDamageTree* old_tree,
    const gfx::RectF& pixel_damage_rect) {
  // This is the plane that will be updated this frame. It may be the root plane
  // or a child plane.
  linked_ptr<OverlayPlane> plane_for_swap;

  // If the frame's size changed, if we haven't updated the root layer, if
  // we have full damage, or if we don't support remote layers, then use the
  // root layer directly.
  if (!allow_partial_swap_ || !old_tree ||
      old_tree->root_plane_->pixel_frame_rect !=
          root_plane_->pixel_frame_rect ||
      pixel_damage_rect == root_plane_->pixel_frame_rect) {
    plane_for_swap = root_plane_;
  }

  // Walk though the old tree's partial damage layers and see if there is one
  // that is appropriate to re-use.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty()) {
    gfx::RectF plane_to_reuse_dip_enlarged_rect;

    // Find the last partial damage plane to re-use the CALayer from. Grow the
    // new rect for this layer to include this damage, and all nearby partial
    // damage layers.
    linked_ptr<OverlayPlane> plane_to_reuse;
    for (auto& old_plane : old_tree->partial_damage_planes_) {
      gfx::RectF dip_enlarged_rect = old_plane->pixel_frame_rect;
      dip_enlarged_rect.Union(pixel_damage_rect);

      // Compute the fraction of the pixels that would not be updated by this
      // swap. If it is too big, try another layer.
      float waste_fraction = dip_enlarged_rect.size().GetArea() * 1.f /
                             pixel_damage_rect.size().GetArea();
      if (waste_fraction > kMaximumPartialDamageWasteFraction)
        continue;

      plane_to_reuse = old_plane;
      plane_to_reuse_dip_enlarged_rect.Union(dip_enlarged_rect);
    }

    if (plane_to_reuse.get()) {
      gfx::RectF enlarged_contents_rect = plane_to_reuse_dip_enlarged_rect;
      enlarged_contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                                   1. / root_plane_->pixel_frame_rect.height());

      plane_for_swap = OverlayPlane::CreateWithFrameRect(
          0, root_plane_->io_surface, plane_to_reuse_dip_enlarged_rect,
          enlarged_contents_rect);

      plane_for_swap->TakeCALayerFrom(plane_to_reuse.get());
      if (plane_to_reuse != old_tree->partial_damage_planes_.back())
        [plane_for_swap->ca_layer removeFromSuperlayer];
    }
  }

  // If we haven't found an appropriate layer to re-use, create a new one, if
  // we haven't already created too many.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty() &&
      old_tree->partial_damage_planes_.size() < kMaximumPartialDamageLayers) {
    gfx::RectF contents_rect = gfx::RectF(pixel_damage_rect);
    contents_rect.Scale(1. / root_plane_->pixel_frame_rect.width(),
                        1. / root_plane_->pixel_frame_rect.height());
    plane_for_swap = OverlayPlane::CreateWithFrameRect(
        0, root_plane_->io_surface, pixel_damage_rect, contents_rect);
  }

  // And if we still don't have a layer, use the root layer.
  if (!plane_for_swap.get() && !pixel_damage_rect.IsEmpty())
    plane_for_swap = root_plane_;

  // Walk all old partial damage planes. Remove anything that is now completely
  // covered, and move everything else into the new |partial_damage_planes_|.
  if (old_tree) {
    for (auto& old_plane : old_tree->partial_damage_planes_) {
      // Intersect the planes' frames with the new root plane to ensure that
      // they don't get kept alive inappropriately.
      gfx::RectF old_plane_frame_rect = old_plane->pixel_frame_rect;
      old_plane_frame_rect.Intersect(root_plane_->pixel_frame_rect);

      bool old_plane_covered_by_swap = false;
      if (plane_for_swap.get() &&
          plane_for_swap->pixel_frame_rect.Contains(old_plane_frame_rect)) {
        old_plane_covered_by_swap = true;
      }
      if (!old_plane_covered_by_swap) {
        DCHECK(old_plane->ca_layer);
        partial_damage_planes_.push_back(old_plane);
      }
    }
    if (plane_for_swap != root_plane_)
      root_plane_ = old_tree->root_plane_;
  }

  // Finally, add the new swap's plane at the back of the list, if it exists.
  if (plane_for_swap.get() && plane_for_swap != root_plane_) {
    partial_damage_planes_.push_back(plane_for_swap);
  }
}

void CALayerPartialDamageTree::UpdateRootAndPartialDamageCALayers(
    CALayer* superlayer,
    float scale_factor) {
  if (!allow_partial_swap_) {
    DCHECK(partial_damage_planes_.empty());
    return;
  }

  // Allocate and update CALayers for the backbuffer and partial damage layers.
  if (!root_plane_->ca_layer) {
    root_plane_->ca_layer.reset([[CALayer alloc] init]);
    [superlayer setSublayers:nil];
    [superlayer addSublayer:root_plane_->ca_layer];
  }
  for (auto& plane : partial_damage_planes_) {
    if (!plane->ca_layer) {
      DCHECK(plane == partial_damage_planes_.back());
      plane->ca_layer.reset([[CALayer alloc] init]);
    }
    if (![plane->ca_layer superlayer]) {
      DCHECK(plane == partial_damage_planes_.back());
      [superlayer addSublayer:plane->ca_layer];
    }
  }
  root_plane_->UpdateProperties(scale_factor);
  for (auto& plane : partial_damage_planes_)
    plane->UpdateProperties(scale_factor);
}

CALayerPartialDamageTree::CALayerPartialDamageTree(
    bool allow_partial_swap,
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface,
    const gfx::Rect& pixel_frame_rect)
    : allow_partial_swap_(allow_partial_swap) {
  root_plane_ = OverlayPlane::CreateWithFrameRect(
      0, io_surface, gfx::RectF(pixel_frame_rect), gfx::RectF(0, 0, 1, 1));
}

CALayerPartialDamageTree::~CALayerPartialDamageTree() {}

base::ScopedCFTypeRef<IOSurfaceRef>
CALayerPartialDamageTree::RootLayerIOSurface() {
  return root_plane_->io_surface;
}

void CALayerPartialDamageTree::CommitCALayers(
    CALayer* superlayer,
    scoped_ptr<CALayerPartialDamageTree> old_tree,
    float scale_factor,
    const gfx::Rect& pixel_damage_rect) {
  UpdateRootAndPartialDamagePlanes(old_tree.get(),
                                   gfx::RectF(pixel_damage_rect));
  UpdateRootAndPartialDamageCALayers(superlayer, scale_factor);
}

}  // namespace content
