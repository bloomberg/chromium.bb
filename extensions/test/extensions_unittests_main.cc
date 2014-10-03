// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "content/public/common/content_client.h"
#include "content/public/test/content_test_suite_base.h"
#include "content/public/test/unittest_test_suite.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/test/test_extensions_client.h"
#include "mojo/embedder/test_embedder.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gl/gl_surface.h"

namespace {

// Content client that exists only to register chrome-extension:// scheme with
// the url module.
// TODO(jamescook): Should this be merged with ShellContentClient? Should this
// be a persistent object available to tests?
class ExtensionsContentClient : public content::ContentClient {
 public:
  ExtensionsContentClient() {}
  virtual ~ExtensionsContentClient() {}

  // content::ContentClient overrides:
  virtual void AddAdditionalSchemes(
      std::vector<std::string>* standard_schemes,
      std::vector<std::string>* savable_schemes) OVERRIDE {
    standard_schemes->push_back(extensions::kExtensionScheme);
    savable_schemes->push_back(extensions::kExtensionScheme);
    standard_schemes->push_back(extensions::kExtensionResourceScheme);
    savable_schemes->push_back(extensions::kExtensionResourceScheme);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionsContentClient);
};

// The test suite for extensions_unittests.
class ExtensionsTestSuite : public content::ContentTestSuiteBase {
 public:
  ExtensionsTestSuite(int argc, char** argv);
  virtual ~ExtensionsTestSuite();

 private:
  // base::TestSuite:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  scoped_ptr<extensions::TestExtensionsClient> client_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsTestSuite);
};

ExtensionsTestSuite::ExtensionsTestSuite(int argc, char** argv)
    : content::ContentTestSuiteBase(argc, argv) {}

ExtensionsTestSuite::~ExtensionsTestSuite() {}

void ExtensionsTestSuite::Initialize() {
  content::ContentTestSuiteBase::Initialize();
  gfx::GLSurface::InitializeOneOffForTests();

  // Register the chrome-extension:// scheme via this circuitous path. Note
  // that this does not persistently set up a ContentClient; individual tests
  // must use content::SetContentClient().
  {
    ExtensionsContentClient content_client;
    RegisterContentSchemes(&content_client);
  }

  extensions::RegisterPathProvider();

  base::FilePath extensions_shell_and_test_pak_path;
  PathService::Get(base::DIR_MODULE, &extensions_shell_and_test_pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak"));

  client_.reset(new extensions::TestExtensionsClient());
  extensions::ExtensionsClient::Set(client_.get());
}

void ExtensionsTestSuite::Shutdown() {
  extensions::ExtensionsClient::Set(NULL);
  client_.reset();

  ui::ResourceBundle::CleanupSharedInstance();
  content::ContentTestSuiteBase::Shutdown();
}

}  // namespace

int main(int argc, char** argv) {
  content::UnitTestTestSuite test_suite(new ExtensionsTestSuite(argc, argv));

  mojo::embedder::test::InitWithSimplePlatformSupport();
  return base::LaunchUnitTests(argc,
                               argv,
                               base::Bind(&content::UnitTestTestSuite::Run,
                                          base::Unretained(&test_suite)));
}
