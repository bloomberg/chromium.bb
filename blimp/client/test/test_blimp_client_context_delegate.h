// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
#define BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_

#include "base/macros.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

namespace blimp {
namespace client {
class BlimpContents;

// Helper class to use in tests when a BlimpClientContextDelegate is needed.
class TestBlimpClientContextDelegate : public BlimpClientContextDelegate {
 public:
  TestBlimpClientContextDelegate();
  ~TestBlimpClientContextDelegate() override;

  // BlimpClientContextDelegate implementation.
  void AttachBlimpContentsHelpers(BlimpContents* blimp_contents) override;

  // Returns the last value of the parameter given to a call to
  // AttachBlimpContentsHelpers.
  BlimpContents* GetBlimpContentsWithLastAttachedHelpers();

 private:
  // The last value of the parameter given to a call to
  // AttachBlimpContentsHelpers.
  BlimpContents* blimp_contents_with_last_attached_helpers_;

  DISALLOW_COPY_AND_ASSIGN(TestBlimpClientContextDelegate);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_TEST_BLIMP_CLIENT_CONTEXT_DELEGATE_H_
