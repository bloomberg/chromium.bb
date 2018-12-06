// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/mojo_web_test_helper.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {

MojoWebTestHelper::MojoWebTestHelper() {}

MojoWebTestHelper::~MojoWebTestHelper() {}

// static
void MojoWebTestHelper::Create(mojom::MojoWebTestHelperRequest request) {
  mojo::MakeStrongBinding(std::make_unique<MojoWebTestHelper>(),
                          std::move(request));
}

void MojoWebTestHelper::Reverse(const std::string& message,
                                ReverseCallback callback) {
  std::move(callback).Run(std::string(message.rbegin(), message.rend()));
}

}  // namespace content
