// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_MOJO_WEB_TEST_HELPER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_MOJO_WEB_TEST_HELPER_H_

#include "base/macros.h"
#include "content/test/data/mojo_layouttest_test.mojom.h"

namespace content {

class MojoWebTestHelper : public mojom::MojoLayoutTestHelper {
 public:
  MojoWebTestHelper();
  ~MojoWebTestHelper() override;

  static void Create(mojom::MojoLayoutTestHelperRequest request);

  // mojom::MojoLayoutTestHelper:
  void Reverse(const std::string& message, ReverseCallback callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoWebTestHelper);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_MOJO_WEB_TEST_HELPER_H_
