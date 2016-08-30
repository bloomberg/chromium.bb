// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_ANDROID_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_ANDROID_H_

#include "base/macros.h"
#include "blimp/client/core/contents/blimp_contents_view.h"
#include "ui/android/view_android.h"

namespace blimp {
namespace client {
class BlimpContentsImplAndroid;

class BlimpContentsViewAndroid : public BlimpContentsView {
 public:
  explicit BlimpContentsViewAndroid(BlimpContentsImplAndroid* blimp_contents);

  // BlimpContentsView implementation.
  gfx::NativeView GetNativeView() override;

 private:
  ui::ViewAndroid view_;

  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_ANDROID_H_
