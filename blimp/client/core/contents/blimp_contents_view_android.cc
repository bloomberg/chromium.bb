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
    BlimpContentsImpl* blimp_contents) {
  return base::MakeUnique<BlimpContentsViewAndroid>(
      blimp_contents->GetBlimpContentsImplAndroid());
}

BlimpContentsViewAndroid::BlimpContentsViewAndroid(
    BlimpContentsImplAndroid* blimp_contents) {
  // TODO(khushalsagar): Get the ViewAndroidDelegate from java after it has a
  // BlimpView. Also get the WindowAndroid so this view can add itself as a
  // child to it.
  // TODO(dtrainor): Use the layer from the compositor manager here instead when
  // it goes in the BlimpContents.
  view_.SetLayer(cc::Layer::Create());
}

gfx::NativeView BlimpContentsViewAndroid::GetNativeView() {
  return &view_;
}

}  // namespace client
}  // namespace blimp
