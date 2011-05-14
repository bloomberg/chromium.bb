// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_
#define CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/browser/resource_context.h"

namespace base {
template <typename T>
struct DefaultLazyInstanceTraits;
}  // namespace base

namespace content {

class MockResourceContext : public ResourceContext {
 public:
  static const ResourceContext& GetInstance();

 private:
  friend struct base::DefaultLazyInstanceTraits<MockResourceContext>;

  MockResourceContext();
  ~MockResourceContext();
  virtual void EnsureInitialized() const OVERRIDE;

  const scoped_refptr<net::URLRequestContext> test_request_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOCK_RESOURCE_CONTEXT_H_
