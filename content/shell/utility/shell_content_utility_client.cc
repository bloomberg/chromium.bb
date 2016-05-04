// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include <memory>

#include "base/bind.h"
#include "content/public/test/test_mojo_app.h"

namespace content {

namespace {

std::unique_ptr<shell::ShellClient> CreateTestApp(
    const base::Closure& quit_closure) {
  return std::unique_ptr<shell::ShellClient>(new TestMojoApp);
}

}  // namespace

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::RegisterMojoApplications(
    StaticMojoApplicationMap* apps) {
  apps->insert(std::make_pair(kTestMojoAppUrl, base::Bind(&CreateTestApp)));
}

}  // namespace content
