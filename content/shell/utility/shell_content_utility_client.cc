// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/utility/shell_content_utility_client.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/test/test_mojo_app.h"
#include "mojo/shell/static_application_loader.h"

namespace content {

namespace {

scoped_ptr<mojo::ApplicationDelegate> CreateTestApp() {
  return scoped_ptr<mojo::ApplicationDelegate>(new TestMojoApp);
}

}  // namespace

ShellContentUtilityClient::~ShellContentUtilityClient() {
}

void ShellContentUtilityClient::RegisterMojoApplications(
    StaticMojoApplicationMap* apps) {
  apps->insert(
      std::make_pair(GURL(kTestMojoAppUrl), base::Bind(&CreateTestApp)));
}

}  // namespace content
