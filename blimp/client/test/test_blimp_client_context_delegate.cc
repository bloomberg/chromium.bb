// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/test_blimp_client_context_delegate.h"

#include "blimp/client/public/blimp_client_context_delegate.h"

namespace blimp {
namespace client {

TestBlimpClientContextDelegate::TestBlimpClientContextDelegate()
    : blimp_contents_with_last_attached_helpers_() {}

TestBlimpClientContextDelegate::~TestBlimpClientContextDelegate() {}

void TestBlimpClientContextDelegate::AttachBlimpContentsHelpers(
    BlimpContents* blimp_contents) {
  blimp_contents_with_last_attached_helpers_ = blimp_contents;
}

BlimpContents*
TestBlimpClientContextDelegate::GetBlimpContentsWithLastAttachedHelpers() {
  return blimp_contents_with_last_attached_helpers_;
}

}  // namespace client
}  // namespace blimp
