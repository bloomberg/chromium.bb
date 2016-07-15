// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_client_context_impl.h"

#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/blimp_contents.h"
#include "blimp/client/test/test_blimp_client_context_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace client {
namespace {

TEST(BlimpClientContextImpl, CreatedBlimpContentsGetsHelpersAttached) {
  BlimpClientContextImpl blimp_client_context;
  TestBlimpClientContextDelegate delegate;
  blimp_client_context.SetDelegate(&delegate);

  std::unique_ptr<BlimpContents> blimp_contents =
      blimp_client_context.CreateBlimpContents();
  DCHECK(blimp_contents);
  DCHECK_EQ(blimp_contents.get(),
            delegate.GetBlimpContentsWithLastAttachedHelpers());
}

}  // namespace
}  // namespace client
}  // namespace blimp
