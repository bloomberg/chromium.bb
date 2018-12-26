// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace content {

namespace {
enum class ServicifiedFeatures { kNone, kServiceWorker };
}

// Tests POST requests that include a file and are intercepted by a service
// worker. This is a browser test rather than a web test because as
// https://crbug.com/786510 describes, http tests involving file uploads usually
// need to be in the http/tests/local directory, which does tricks with origins
// that can break when Site Isolation is enabled.
class ServiceWorkerFileUploadTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<ServicifiedFeatures> {
 public:
  ServiceWorkerFileUploadTest() = default;

  void SetUp() override {
    ServicifiedFeatures param = GetParam();
    switch (param) {
      case ServicifiedFeatures::kNone:
        scoped_feature_list_.InitAndDisableFeature(
            blink::features::kServiceWorkerServicification);
        break;
      case ServicifiedFeatures::kServiceWorker:
        scoped_feature_list_.InitAndEnableFeature(
            blink::features::kServiceWorkerServicification);
        break;
    }

    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    ContentBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Make all hosts resolve to 127.0.0.1 so the same embedded test server can
    // be used for cross-origin URLs.
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFileUploadTest);
};

// Tests using Request.formData() from a service worker for a navigation
// request due to a form submission to a cross-origin target. Regression
// test for https://crbug.com/916070.
IN_PROC_BROWSER_TEST_P(ServiceWorkerFileUploadTest, AsFormData) {
  // Install the service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            embedded_test_server()->GetURL(
                                "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(shell(), "register('file_upload_worker.js');"));

  // Generate a cross-origin URL for the page with a file upload form.
  GURL top_frame_url =
      embedded_test_server()->GetURL("/service_worker/form.html");
  GURL::Replacements replacements;
  replacements.SetHostStr("cross-origin.example.com");
  top_frame_url = top_frame_url.ReplaceComponents(replacements);

  // Also add "target=" query parameter so the page submits the form back to the
  // original origin with the service worker.
  GURL target_url = embedded_test_server()->GetURL("/service_worker/upload");
  top_frame_url =
      net::AppendQueryParameter(top_frame_url, "target", target_url.spec());

  // Navigate to the cross-origin page with a file upload form.
  EXPECT_TRUE(NavigateToURL(shell(), top_frame_url));

  // Prepare a file for the upload form.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("come on baby america");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  // Fill out the form to refer to the test file.
  base::RunLoop run_loop;
  auto delegate =
      std::make_unique<FileChooserDelegate>(file_path, run_loop.QuitClosure());
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecJs(shell(), "fileInput.click();"));
  run_loop.Run();

  // Submit the form.
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecJs(shell(), "form.submit();"));
  form_post_observer.Wait();

  // Extract the body payload. It is JSON describing the uploaded form content.
  EvalJsResult result = EvalJs(shell()->web_contents()->GetMainFrame(),
                               "document.body.textContent");
  ASSERT_TRUE(result.error.empty());
  std::unique_ptr<base::DictionaryValue> dict = base::DictionaryValue::From(
      base::JSONReader::Read(result.ExtractString()));
  ASSERT_TRUE(dict) << "not json: " << result.ExtractString();
  auto* entries_value = dict->FindKeyOfType("entries", base::Value::Type::LIST);
  ASSERT_TRUE(entries_value);
  // There was one entry uploaded so far.
  const base::Value::ListStorage& entries = entries_value->GetList();
  ASSERT_EQ(1u, entries.size());

  // Test the first entry of the form data.

  // The key should be "file" (the <input type="file"> element's name).
  const auto* key0 = entries[0].FindKeyOfType("key", base::Value::Type::STRING);
  ASSERT_TRUE(key0);
  EXPECT_EQ("file", key0->GetString());

  // The value should describe the uploaded file.
  const auto* value0 =
      entries[0].FindKeyOfType("value", base::Value::Type::DICTIONARY);
  ASSERT_TRUE(value0);
  // "type" is "file" since the value was a File object.
  auto* value0_type = value0->FindKeyOfType("type", base::Value::Type::STRING);
  EXPECT_EQ("file", value0_type->GetString());
  // "name" is the filename (File.name)
  auto* value0_name = value0->FindKeyOfType("name", base::Value::Type::STRING);
  EXPECT_EQ(file_path.BaseName().MaybeAsASCII(), value0_name->GetString());
  // "size" is the file size (File.size)
  auto* value0_size = value0->FindKeyOfType("size", base::Value::Type::INTEGER);
  ASSERT_GE(value0_size->GetInt(), 0);
  EXPECT_EQ(file_content.size(),
            base::checked_cast<size_t>(value0_size->GetInt()));
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        ServiceWorkerFileUploadTest,
                        ::testing::Values(ServicifiedFeatures::kNone,
                                          ServicifiedFeatures::kServiceWorker));

}  // namespace content
