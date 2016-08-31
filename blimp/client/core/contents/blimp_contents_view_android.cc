// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/contents/blimp_contents_view_android.h"

#include "base/memory/ptr_util.h"
#include "blimp/client/core/contents/android/blimp_contents_impl_android.h"
#include "cc/layers/layer.h"

namespace blimp {
namespace client {

// static
std::unique_ptr<BlimpContentsView> BlimpContentsView::Create(
    BlimpContentsImpl* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer) {
  return base::MakeUnique<BlimpContentsViewAndroid>(
      blimp_contents->GetBlimpContentsImplAndroid(), contents_layer);
}

BlimpContentsViewAndroid::BlimpContentsViewAndroid(
    BlimpContentsImplAndroid* blimp_contents,
    scoped_refptr<cc::Layer> contents_layer) {
  // TODO(khushalsagar): Get the ViewAndroidDelegate from java after it has a
  // BlimpView. Also get the WindowAndroid so this view can add itself as a
  // child to it.
  view_.SetLayer(contents_layer);
}

gfx::NativeView BlimpContentsViewAndroid::GetNativeView() {
  return &view_;
}

}  // namespace client
}  // namespace blimp
