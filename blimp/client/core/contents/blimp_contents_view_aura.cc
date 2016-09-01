// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_view_aura.h"

#include "base/memory/ptr_util.h"
#include "cc/layers/layer.h"

namespace blimp {
namespace client {

// static
std::unique_ptr<BlimpContentsView> BlimpContentsView::Create(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer) {
  return base::MakeUnique<BlimpContentsViewAura>();
}

BlimpContentsViewAura::BlimpContentsViewAura() {}

gfx::NativeView BlimpContentsViewAura::GetNativeView() {
  return nullptr;
}

ImeFeature::Delegate* BlimpContentsViewAura::GetImeDelegate() {
  return nullptr;
}

}  // namespace client
}  // namespace blimp
