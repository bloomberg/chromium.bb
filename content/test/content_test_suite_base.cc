// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_test_suite_base.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/test_suite.h"
#include "content/common/url_schemes.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "media/base/media.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor_setup.h"

namespace content {

ContentTestSuiteBase::ContentTestSuiteBase(int argc, char** argv)
    : base::TestSuite(argc, argv) {
}

void ContentTestSuiteBase::Initialize() {
  base::TestSuite::Initialize();
  media::InitializeMediaLibraryForTesting();

  scoped_ptr<ContentClient> client_for_init(CreateClientForInitialization());
  SetContentClient(client_for_init.get());
  RegisterContentSchemes(false);
  SetContentClient(NULL);

  RegisterPathProvider();
  ui::RegisterPathProvider();

  // Mock out the compositor on platforms that use it.
  ui::SetupTestCompositor();
}

}  // namespace content
