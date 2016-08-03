// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_
#define BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_

#include "base/macros.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/contents/blimp_contents.h"

namespace blimp {
namespace client {

// A dummy implementation of the BlimpClientContext.
class DummyBlimpClientContext : public BlimpClientContext {
 public:
  DummyBlimpClientContext();
  ~DummyBlimpClientContext() override;

  // BlimpClientContext implementation.
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyBlimpClientContext);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_
