// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/test/result_catcher.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "url/gurl.h"

namespace {

scoped_ptr<net::ClientCertStore> CreateNullCertStore() {
  return nullptr;
}

void InstallNullCertStoreFactoryOnIOThread(
    content::ResourceContext* resource_context) {
  ProfileIOData::FromResourceContext(resource_context)
      ->set_client_cert_store_factory_for_testing(
          base::Bind(&CreateNullCertStore));
}

}  // namespace

class BackgroundXhrTest : public ExtensionBrowserTest {
 protected:
  void RunTest(const std::string& path, const GURL& url) {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("background_xhr"));
    ASSERT_TRUE(extension);

    extensions::ResultCatcher catcher;
    GURL test_url = net::AppendQueryParameter(extension->GetResourceURL(path),
                                              "url", url.spec());
    ui_test_utils::NavigateToURL(browser(), test_url);
    ASSERT_TRUE(catcher.GetNextResult());
  }
};

// Test that fetching a URL using TLS client auth doesn't crash, hang, or
// prompt.
IN_PROC_BROWSER_TEST_F(BackgroundXhrTest, TlsClientAuth) {
  // Install a null ClientCertStore so the client auth prompt isn't bypassed due
  // to the system certificate store returning no certificates.
  base::RunLoop loop;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&InstallNullCertStoreFactoryOnIOThread,
                 browser()->profile()->GetResourceContext()),
      loop.QuitClosure());
  loop.Run();

  // Launch HTTPS server.
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_tls_client_auth.html", https_server.GetURL("")));
}

// Test that fetching a URL using HTTP auth doesn't crash, hang, or prompt.
IN_PROC_BROWSER_TEST_F(BackgroundXhrTest, HttpAuth) {
  ASSERT_TRUE(test_server()->Start());
  ASSERT_NO_FATAL_FAILURE(
      RunTest("test_http_auth.html", test_server()->GetURL("auth-basic")));
}
