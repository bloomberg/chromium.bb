// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/mojo/static_loader.h"
#include "content/public/test/test_mojo_app.h"

namespace content {

namespace {

scoped_ptr<mojo::ShellClient> CreateTestApp() {
  return scoped_ptr<mojo::ShellClient>(new TestMojoApp);
}

}  // namespace

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::RegisterMojoApplications(
    StaticMojoApplicationMap* apps) {
  apps->insert(std::make_pair(kTestMojoAppUrl, base::Bind(&CreateTestApp)));
}

}  // namespace content
