// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_AURA_H_
#define BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_AURA_H_

#include "base/macros.h"
#include "blimp/client/core/contents/blimp_contents_view.h"

namespace blimp {
namespace client {

class BlimpContentsViewAura : public BlimpContentsView {
 public:
  BlimpContentsViewAura();

  // BlimpContentsView implementation.
  gfx::NativeView GetNativeView() override;
  ImeFeature::Delegate* GetImeDelegate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpContentsViewAura);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_BLIMP_CONTENTS_VIEW_AURA_H_
