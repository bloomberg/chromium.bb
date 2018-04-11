// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOST_TRANSFORMER_HELPER_H_
#define ASH_HOST_TRANSFORMER_HELPER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"

namespace gfx {
class Insets;
class Rect;
class Size;
class Transform;
}

namespace ash {
class AshWindowTreeHost;
class RootWindowTransformer;

// A helper class to handle ash specific feature that requires
// transforming a root window (such as rotation, UI zooming).
class ASH_EXPORT TransformerHelper {
 public:
  explicit TransformerHelper(AshWindowTreeHost* ash_host);
  ~TransformerHelper();

  // Initializes the transformer with identity transform.
  void Init();

  // Returns the the insets that specifies the effective root window
  // area within the host window.
  gfx::Insets GetHostInsets() const;

  // Sets a simple transform with no host insets.
  void SetTransform(const gfx::Transform& transform);

  // Sets a RootWindowTransformer which takes the insets into account.
  void SetRootWindowTransformer(
      std::unique_ptr<RootWindowTransformer> transformer);

  // Returns the transforms applied to the root window.
  gfx::Transform GetTransform() const;
  gfx::Transform GetInverseTransform() const;

  // Returns the transformed root window bounds based on the host size and
  // current transform.
  gfx::Rect GetTransformedWindowBounds(const gfx::Size& host_size) const;

 private:
  AshWindowTreeHost* ash_host_;
  std::unique_ptr<RootWindowTransformer> transformer_;

  DISALLOW_COPY_AND_ASSIGN(TransformerHelper);
};

}  // namespace ash

#endif  // ASH_HOST_TRANSFORMER_HELPER_H_
