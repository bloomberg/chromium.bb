// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/updater/update_service.h"
#include "extensions/common/extension_urls.h"
#include "extensions/shell/test/shell_test.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"

namespace extensions {

namespace {

using FakeResponse = std::pair<std::string, net::HttpStatusCode>;

// TODO(rockot): In general there's enough mock-Omaha-noise that this might be
// better placed into some test library code in //components/update_client.
FakeResponse CreateFakeUpdateResponse(const std::string& id,
                                      size_t crx_length) {
  std::string response_text = base::StringPrintf(
      "<gupdate xmlns=\"http://www.google.com/update2/response\" "
      "    protocol=\"2.0\" server=\"prod\">\n"
      "  <daystart elapsed_days=\"2860\" elapsed_seconds=\"42042\"/>\n"
      "  <app appid=\"%s\" status=\"ok\">\n"
      "    <updatecheck codebase=\"%s\" fp=\"0\" hash=\"\" hash_sha256=\"\" "
      "        size=\"%d\" status=\"ok\" version=\"1\"/>\n"
      "  </app>\n"
      "</gupdate>\n",
      id.c_str(),
      base::StringPrintf("https://fake-omaha-hostname/%s.crx",
                         id.c_str()).c_str(),
      static_cast<int>(crx_length));
  return std::make_pair(response_text, net::HTTP_OK);
}

FakeResponse CreateFakeUpdateNotFoundResponse() {
  return std::make_pair(
      std::string(
          "<gupdate xmlns=\"http://www.google.com/update2/response\" "
          "         protocol=\"2.0\" server=\"prod\">\n"
          "  <daystart elapsed_days=\"4242\" elapsed_seconds=\"42042\"/>\n"
          "  <app appid=\"\" status=\"error-invalidAppId\">\n"
          "</gupdate>"),
      net::HTTP_OK);
}

bool ExtractKeyValueFromComponent(const std::string& component_str,
                                  const std::string& target_key,
                                  std::string* extracted_value) {
  url::Component component(0, static_cast<int>(component_str.length()));
  url::Component key, value;
  while (url::ExtractQueryKeyValue(component_str.c_str(), &component, &key,
                                   &value)) {
    if (target_key == component_str.substr(key.begin, key.len)) {
      *extracted_value = component_str.substr(value.begin, value.len);
      return true;
    }
  }
  return false;
}

// This extracts an extension ID from an Omaha update query. Queries have the
// form https://foo/bar/update?x=id%3Dabcdefghijklmnopqrstuvwxyzaaaaaa%26...
// This function extracts the 'x' query parameter (e.g. "id%3Dabcdef...."),
// unescapes its value (to become e.g., "id=abcdef...", and then extracts the
// 'id' value from the result (e.g. "abcdef...").
bool ExtractIdFromUpdateQuery(const std::string& query_str, std::string* id) {
  std::string data_string;
  if (!ExtractKeyValueFromComponent(query_str, "x", &data_string))
    return false;
  data_string = net::UnescapeURLComponent(data_string,
                                          net::UnescapeRule::URL_SPECIAL_CHARS);
  if (!ExtractKeyValueFromComponent(data_string, "id", id))
    return false;
  EXPECT_EQ(32u, id->size());
  return true;
}

void ExpectDownloadSuccess(const base::Closure& continuation, bool success) {
  EXPECT_TRUE(success) << "Download failed.";
  continuation.Run();
}

class FakeUpdateURLFetcherFactory : public net::URLFetcherFactory {
 public:
  ~FakeUpdateURLFetcherFactory() override {}

  void RegisterFakeExtension(const std::string& id,
                             const std::string& contents) {
    CHECK_EQ(32u, id.size());
    fake_extensions_.insert(std::make_pair(id, contents));
  }

  // net::URLFetcherFactory:
  net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate) override {
    if (url.spec().find(extension_urls::GetWebstoreUpdateUrl().spec()) == 0) {
      // Handle fake Omaha requests.
      return CreateUpdateManifestFetcher(url, delegate);
    } else if (url.spec().find("https://fake-omaha-hostname") == 0) {
      // Handle a fake CRX request.
      return CreateCrxFetcher(url, delegate);
    }
    NOTREACHED();
    return nullptr;
  }

 private:
  net::URLFetcher* CreateUpdateManifestFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate) {
    // If we have a fake CRX for the ID, return a fake update blob for it.
    // Otherwise return an invalid-ID response.
    FakeResponse response;
    std::string extension_id;
    if (!ExtractIdFromUpdateQuery(url.query(), &extension_id)) {
      response = CreateFakeUpdateNotFoundResponse();
    } else {
      const auto& iter = fake_extensions_.find(extension_id);
      if (iter == fake_extensions_.end())
        response = CreateFakeUpdateNotFoundResponse();
      else
        response = CreateFakeUpdateResponse(extension_id, iter->second.size());
    }
    return new net::FakeURLFetcher(url, delegate, response.first,
                                   response.second,
                                   net::URLRequestStatus::SUCCESS);
  }

  net::URLFetcher* CreateCrxFetcher(const GURL& url,
                                    net::URLFetcherDelegate* delegate) {
    FakeResponse response;
    std::string extension_id = url.path().substr(1, 32);
    const auto& iter = fake_extensions_.find(extension_id);
    if (iter == fake_extensions_.end())
      response = std::make_pair(std::string(), net::HTTP_NOT_FOUND);
    else
      response = std::make_pair(iter->second, net::HTTP_OK);
    net::TestURLFetcher* fetcher =
        new net::FakeURLFetcher(url, delegate, response.first, response.second,
                                net::URLRequestStatus::SUCCESS);
    fetcher->SetResponseFilePath(base::FilePath::FromUTF8Unsafe(url.path()));
    return fetcher;
  }

  std::map<std::string, std::string> fake_extensions_;
};

}  // namespace

class UpdateServiceTest : public AppShellTest {
 public:
  UpdateServiceTest() {}
  ~UpdateServiceTest() override {}

  void SetUpOnMainThread() override {
    AppShellTest::SetUpOnMainThread();

    update_service_ = UpdateService::Get(browser_context());

    default_url_fetcher_factory_.reset(new FakeUpdateURLFetcherFactory());
    url_fetcher_factory_.reset(
        new net::FakeURLFetcherFactory(default_url_fetcher_factory_.get()));
  }

 protected:
  void RegisterFakeExtension(const std::string& id,
                             const std::string& crx_contents) {
    default_url_fetcher_factory_->RegisterFakeExtension(id, crx_contents);
  }

  UpdateService* update_service() const { return update_service_; }

 private:
  scoped_ptr<FakeUpdateURLFetcherFactory> default_url_fetcher_factory_;
  scoped_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;

  UpdateService* update_service_;
};

IN_PROC_BROWSER_TEST_F(UpdateServiceTest, DownloadAndInstall) {
  const char kCrxId[] = "abcdefghijklmnopqrstuvwxyzaaaaaa";
  const char kCrxContents[] = "Hello, world!";
  RegisterFakeExtension(kCrxId, kCrxContents);

  base::RunLoop run_loop;
  update_service()->DownloadAndInstall(
      kCrxId, base::Bind(ExpectDownloadSuccess, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace extensions
