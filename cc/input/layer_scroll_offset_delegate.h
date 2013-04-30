// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_LAYER_SCROLL_OFFSET_DELEGATE_H_
#define CC_INPUT_LAYER_SCROLL_OFFSET_DELEGATE_H_

namespace cc {

// The LayerScrollOffsetDelegate allows for the embedder to take ownership of
// the scroll offset of the root layer.
//
// The LayerScrollOffsetDelegate is only used on the impl thread.
class LayerScrollOffsetDelegate {
 public:
  // This is called by the compositor when the scroll offset of the layer would
  // have otherwise changed.
  virtual void SetTotalScrollOffset(gfx::Vector2dF new_value) = 0;

  // This is called by the compositor to query the current scroll offset of the
  // layer.
  // There is no requirement that the return values of this method are
  // stable in time (two subsequent calls may yield different results).
  // The return value is not required to be related to the values passed in to
  // the SetTotalScrollOffset method in any way.
  virtual gfx::Vector2dF GetTotalScrollOffset() = 0;

 protected:
  LayerScrollOffsetDelegate() {}
  virtual ~LayerScrollOffsetDelegate() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LayerScrollOffsetDelegate);
};

}  // namespace cc

#endif  // CC_INPUT_LAYER_SCROLL_OFFSET_DELEGATE_H_
