// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_SURFACE_SEQUENCE_GENERATOR_H_
#define CC_TREES_SURFACE_SEQUENCE_GENERATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "cc/base/cc_export.h"

namespace cc {
struct SurfaceSequence;

// Generates unique surface sequences for a surface client id.
class CC_EXPORT SurfaceSequenceGenerator {
 public:
  SurfaceSequenceGenerator();
  ~SurfaceSequenceGenerator();

  void set_surface_client_id(uint32_t client_id) {
    surface_client_id_ = client_id;
  }

  SurfaceSequence CreateSurfaceSequence();

 private:
  uint32_t surface_client_id_;
  uint32_t next_surface_sequence_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceSequenceGenerator);
};

}  // namespace cc

#endif  // CC_TREES_SURFACE_SEQUENCE_GENERATOR_H_
