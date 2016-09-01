// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "blimp/client/core/contents/ime_feature.h"
#include "ui/gfx/native_widget_types.h"

namespace cc {
class Layer;
}  // namespace cc

namespace blimp {
namespace client {
class BlimpContentsImpl;

class BlimpContentsView {
 public:
  static std::unique_ptr<BlimpContentsView> Create(
      BlimpContentsImpl* blimp_contents,
      scoped_refptr<cc::Layer> contents_layer);

  virtual ~BlimpContentsView() {}

  virtual gfx::NativeView GetNativeView() = 0;
  virtual ImeFeature::Delegate* GetImeDelegate() = 0;
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_H_
