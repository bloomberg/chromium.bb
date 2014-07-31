// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_QUADS_RENDER_PASS_H_
#define CC_QUADS_RENDER_PASS_H_

#include <utility>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_vector.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace base {
namespace debug {
class TracedValue;
}
class Value;
};

namespace cc {

class DrawQuad;
class RenderPassDrawQuad;
class CopyOutputRequest;
class SharedQuadState;

// A list of DrawQuad objects, sorted internally in front-to-back order.
class QuadList : public ScopedPtrVector<DrawQuad> {
 public:
  typedef reverse_iterator BackToFrontIterator;
  typedef const_reverse_iterator ConstBackToFrontIterator;

  inline BackToFrontIterator BackToFrontBegin() { return rbegin(); }
  inline BackToFrontIterator BackToFrontEnd() { return rend(); }
  inline ConstBackToFrontIterator BackToFrontBegin() const { return rbegin(); }
  inline ConstBackToFrontIterator BackToFrontEnd() const { return rend(); }
};

typedef ScopedPtrVector<SharedQuadState> SharedQuadStateList;

class CC_EXPORT RenderPass {
 public:
  struct Id {
    int layer_id;
    int index;

    Id(int layer_id, int index) : layer_id(layer_id), index(index) {}
    void* AsTracingId() const;

    bool operator==(const Id& other) const {
      return layer_id == other.layer_id && index == other.index;
    }
    bool operator!=(const Id& other) const {
      return !(*this == other);
    }
    bool operator<(const Id& other) const {
      return layer_id < other.layer_id ||
          (layer_id == other.layer_id && index < other.index);
    }
  };

  ~RenderPass();

  static scoped_ptr<RenderPass> Create();
  static scoped_ptr<RenderPass> Create(size_t num_layers);

  // A shallow copy of the render pass, which does not include its quads or copy
  // requests.
  scoped_ptr<RenderPass> Copy(Id new_id) const;

  // A deep copy of the render passes in the list including the quads.
  static void CopyAll(const ScopedPtrVector<RenderPass>& in,
                      ScopedPtrVector<RenderPass>* out);

  void SetNew(Id id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target);

  void SetAll(Id id,
              const gfx::Rect& output_rect,
              const gfx::Rect& damage_rect,
              const gfx::Transform& transform_to_root_target,
              bool has_transparent_background);

  void AsValueInto(base::debug::TracedValue* dict) const;

  SharedQuadState* CreateAndAppendSharedQuadState();
  template <typename DrawQuadType>
  DrawQuadType* CreateAndAppendDrawQuad() {
    scoped_ptr<DrawQuadType> draw_quad = make_scoped_ptr(new DrawQuadType);
    quad_list.push_back(draw_quad.template PassAs<DrawQuad>());
    return static_cast<DrawQuadType*>(quad_list.back());
  }

  RenderPassDrawQuad* CopyFromAndAppendRenderPassDrawQuad(
      const RenderPassDrawQuad* quad,
      const SharedQuadState* shared_quad_state,
      RenderPass::Id render_pass_id);
  DrawQuad* CopyFromAndAppendDrawQuad(const DrawQuad* quad,
                                      const SharedQuadState* shared_quad_state);

  // Uniquely identifies the render pass in the compositor's current frame.
  Id id;

  // These are in the space of the render pass' physical pixels.
  gfx::Rect output_rect;
  gfx::Rect damage_rect;

  // Transforms from the origin of the |output_rect| to the origin of the root
  // render pass' |output_rect|.
  gfx::Transform transform_to_root_target;

  // If false, the pixels in the render pass' texture are all opaque.
  bool has_transparent_background;

  // If non-empty, the renderer should produce a copy of the render pass'
  // contents as a bitmap, and give a copy of the bitmap to each callback in
  // this list. This property should not be serialized between compositors, as
  // it only makes sense in the root compositor.
  ScopedPtrVector<CopyOutputRequest> copy_requests;

  QuadList quad_list;
  SharedQuadStateList shared_quad_state_list;

 protected:
  explicit RenderPass(size_t num_layers);
  RenderPass();

 private:
  template <typename DrawQuadType>
  DrawQuadType* CopyFromAndAppendTypedDrawQuad(const DrawQuad* quad) {
    scoped_ptr<DrawQuadType> draw_quad =
        make_scoped_ptr(new DrawQuadType(*DrawQuadType::MaterialCast(quad)));
    quad_list.push_back(draw_quad.template PassAs<DrawQuad>());
    return static_cast<DrawQuadType*>(quad_list.back());
  }

  DISALLOW_COPY_AND_ASSIGN(RenderPass);
};

}  // namespace cc

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_MSVC)
inline size_t hash_value(const cc::RenderPass::Id& key) {
  return base::HashPair(key.layer_id, key.index);
}
#elif defined(COMPILER_GCC)
template<>
struct hash<cc::RenderPass::Id> {
  size_t operator()(cc::RenderPass::Id key) const {
    return base::HashPair(key.layer_id, key.index);
  }
};
#else
#error define a hash function for your compiler
#endif  // COMPILER
}  // namespace BASE_HASH_NAMESPACE

namespace cc {
typedef ScopedPtrVector<RenderPass> RenderPassList;
typedef base::hash_map<RenderPass::Id, RenderPass*> RenderPassIdHashMap;
}  // namespace cc

#endif  // CC_QUADS_RENDER_PASS_H_
