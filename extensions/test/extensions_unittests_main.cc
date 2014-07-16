// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "content/public/test/unittest_test_suite.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/test_extensions_client.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class ExtensionsTestSuite : public base::TestSuite {
 public:
  ExtensionsTestSuite(int argc, char** argv);

 private:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  scoped_ptr<extensions::TestExtensionsClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTestSuite);
};

ExtensionsTestSuite::ExtensionsTestSuite(int argc, char** argv)
    : base::TestSuite(argc, argv) {}

void ExtensionsTestSuite::Initialize() {
  base::TestSuite::Initialize();

  extensions::RegisterPathProvider();

  base::FilePath resources_pack_path;
  PathService::Get(base::DIR_MODULE, &resources_pack_path);
  ResourceBundle::InitSharedInstanceWithPakPath(
      resources_pack_path.AppendASCII("extensions_unittests_resources.pak"));

  client_.reset(new extensions::TestExtensionsClient());
  extensions::ExtensionsClient::Set(client_.get());
}

void ExtensionsTestSuite::Shutdown() {
  ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(new ExtensionsTestSuite(argc, argv));

  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&content::UnitTestTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
