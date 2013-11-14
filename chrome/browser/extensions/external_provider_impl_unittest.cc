// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_impl.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace extensions {

namespace {

using namespace net::test_server;

const char kManifestPath[] = "/update_manifest";
const char kAppPath[] = "/app.crx";

class ExternalProviderImplTest : public ExtensionServiceTestBase {
 public:
  ExternalProviderImplTest() {}
  virtual ~ExternalProviderImplTest() {}

  void InitServiceWithExternalProviders() {
    InitializeExtensionServiceWithUpdater();

    ProviderCollection providers;
    extensions::ExternalProviderImpl::CreateExternalProviders(
        service_, profile_.get(), &providers);

    for (ProviderCollection::iterator i = providers.begin();
         i != providers.end();
         ++i) {
      service_->AddProviderForTesting(i->release());
    }
  }

  // ExtensionServiceTestBase overrides:
  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();
    test_server_.reset(new EmbeddedTestServer());
    ASSERT_TRUE(test_server_->InitializeAndWaitUntilReady());
    test_server_->RegisterRequestHandler(
        base::Bind(&ExternalProviderImplTest::HandleRequest,
                   base::Unretained(this)));

    CommandLine* cmdline = CommandLine::ForCurrentProcess();
    cmdline->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                               test_server_->GetURL(kManifestPath).spec());
  }

 private:
  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    GURL url = test_server_->GetURL(request.relative_url);
    if (url.path() == kManifestPath) {
      scoped_ptr<BasicHttpResponse> response(new BasicHttpResponse);
      response->set_code(net::HTTP_OK);
      response->set_content(base::StringPrintf(
          "<?xml version='1.0' encoding='UTF-8'?>\n"
          "<gupdate xmlns='http://www.google.com/update2/response' "
              "protocol='2.0'>\n"
          "  <app appid='%s'>\n"
          "    <updatecheck codebase='%s' version='1.0' />\n"
          "  </app>\n"
          "</gupdate>",
          extension_misc::kInAppPaymentsSupportAppId,
          test_server_->GetURL(kAppPath).spec().c_str()));
      response->set_content_type("text/xml");
      return response.PassAs<HttpResponse>();
    }
    if (url.path() == kAppPath) {
      base::FilePath test_data_dir;
      PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
      std::string contents;
      base::ReadFileToString(
          test_data_dir.AppendASCII("extensions/dummyiap.crx"),
          &contents);
      scoped_ptr<BasicHttpResponse> response(new BasicHttpResponse);
      response->set_code(net::HTTP_OK);
      response->set_content(contents);
      return response.PassAs<HttpResponse>();
    }

    return scoped_ptr<HttpResponse>();
  }

  scoped_ptr<EmbeddedTestServer> test_server_;

  DISALLOW_COPY_AND_ASSIGN(ExternalProviderImplTest);
};

}  // namespace

TEST_F(ExternalProviderImplTest, InAppPayments) {
  InitServiceWithExternalProviders();

  service_->CheckForExternalUpdates();
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_CRX_INSTALLER_DONE,
      content::NotificationService::AllSources()).Wait();


  EXPECT_TRUE(service_->GetInstalledExtension(
      extension_misc::kInAppPaymentsSupportAppId));
  EXPECT_TRUE(service_->IsExtensionEnabled(
      extension_misc::kInAppPaymentsSupportAppId));
}

}  // namespace extensions
