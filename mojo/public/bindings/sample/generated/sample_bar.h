// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_BAR_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_BAR_H_

#include "mojo/public/bindings/lib/bindings.h"

namespace sample {

#pragma pack(push, 1)

class Bar {
 public:
  static Bar* New(mojo::Buffer* buf);

  void set_alpha(uint8_t alpha) { alpha_ = alpha; }
  void set_beta(uint8_t beta) { beta_ = beta; }
  void set_gamma(uint8_t gamma) { gamma_ = gamma; }

  uint8_t alpha() const { return alpha_; }
  uint8_t beta() const { return beta_; }
  uint8_t gamma() const { return gamma_; }

 private:
  friend class mojo::internal::ObjectTraits<Bar>;

  Bar();
  ~Bar();  // NOT IMPLEMENTED

  mojo::internal::StructHeader _header_;
  uint8_t alpha_;
  uint8_t beta_;
  uint8_t gamma_;
  uint8_t _pad0_[5];
};
MOJO_COMPILE_ASSERT(sizeof(Bar) == 16, bad_sizeof_Bar);

#pragma pack(pop)

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_BAR_H_
