// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_TRANSFORMER_HELPER_H_
#define ASH_HOST_TRANSFORMER_HELPER_H_

#include "base/memory/scoped_ptr.h"

namespace gfx {
class Insets;
class Size;
class Transform;
}

namespace ash {
class AshWindowTreeHost;
class RootWindowTransformer;

// A helper class to handle ash specific feature that requires
// transforming a root window (such as rotation, UI zooming).
class TransformerHelper {
 public:
  explicit TransformerHelper(AshWindowTreeHost* ash_host);
  ~TransformerHelper();

  // Returns the the insets that specifies the effective root window
  // area within the host window.
  gfx::Insets GetHostInsets() const;

  // Sets a simple transform with no host insets.
  void SetTransform(const gfx::Transform& transform);

  // Sets a RootWindowTransformer which takes the insets into account.
  void SetRootWindowTransformer(scoped_ptr<RootWindowTransformer> transformer);

  // Returns the transforms applied to the root window.
  gfx::Transform GetTransform() const;
  gfx::Transform GetInverseTransform() const;

  // Updates the root window size based on the host size and
  // current transform.
  void UpdateWindowSize(const gfx::Size& host_size);

 private:
  AshWindowTreeHost* ash_host_;
  scoped_ptr<RootWindowTransformer> transformer_;

  DISALLOW_COPY_AND_ASSIGN(TransformerHelper);
};

}  // namespace ash

#endif  // ASH_HOST_TRANSFORMER_HELPER_H_
