// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
#define BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_

#include "base/macros.h"

namespace blimp {
namespace client {
class BlimpContents;

// BlimpClientContextDelegate is how the BlimpClientContext gets the
// functionality it needs from its embedder.
class BlimpClientContextDelegate {
 public:
  virtual ~BlimpClientContextDelegate() = default;

  // Attaches any required base::SupportsUserData::Data to the BlimpContents.
  virtual void AttachBlimpContentsHelpers(BlimpContents* blimp_contents) = 0;

 protected:
  BlimpClientContextDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
