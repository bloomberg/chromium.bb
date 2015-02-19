// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_TILE_DRAW_INFO_H_
#define CC_RESOURCES_TILE_DRAW_INFO_H_

#include "base/memory/scoped_ptr.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/resource_provider.h"
#include "cc/resources/scoped_resource.h"
#include "cc/resources/tile_task_runner.h"

namespace cc {

// This class holds all the state relevant to drawing a tile.
class CC_EXPORT TileDrawInfo {
 public:
  enum Mode { RESOURCE_MODE, SOLID_COLOR_MODE, OOM_MODE };

  TileDrawInfo();
  ~TileDrawInfo();

  Mode mode() const { return mode_; }

  bool IsReadyToDraw() const;

  ResourceProvider::ResourceId resource_id() const {
    DCHECK(mode_ == RESOURCE_MODE);
    DCHECK(resource_);
    return resource_->id();
  }

  gfx::Size resource_size() const {
    DCHECK(mode_ == RESOURCE_MODE);
    DCHECK(resource_);
    return resource_->size();
  }

  SkColor solid_color() const {
    DCHECK(mode_ == SOLID_COLOR_MODE);
    return solid_color_;
  }

  bool contents_swizzled() const {
    DCHECK(resource_);
    return !PlatformColor::SameComponentOrder(resource_->format());
  }

  bool requires_resource() const {
    return mode_ == RESOURCE_MODE || mode_ == OOM_MODE;
  }

  inline bool has_resource() const { return !!resource_; }

  void SetSolidColorForTesting(SkColor color) { set_solid_color(color); }
  void SetResourceForTesting(scoped_ptr<ScopedResource> resource) {
    resource_ = resource.Pass();
  }

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  friend class Tile;
  friend class TileManager;

  void set_use_resource() { mode_ = RESOURCE_MODE; }

  void set_solid_color(const SkColor& color) {
    mode_ = SOLID_COLOR_MODE;
    solid_color_ = color;
  }

  void set_oom() { mode_ = OOM_MODE; }

  Mode mode_;
  SkColor solid_color_;
  scoped_ptr<ScopedResource> resource_;
};

}  // namespace cc

#endif  // CC_RESOURCES_TILE_DRAW_INFO_H_
