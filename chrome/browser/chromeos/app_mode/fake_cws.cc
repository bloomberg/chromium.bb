// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/fake_cws.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "crypto/sha2.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using net::test_server::BasicHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

const char kWebstoreDomain[] = "cws.com";
// Kiosk app crx file download path under web store site.
const char kCrxDownloadPath[] = "/chromeos/app_mode/webstore/downloads/";

}  // namespace

void FakeCWS::Init(EmbeddedTestServer* embedded_test_server) {
  SetupWebStore(embedded_test_server->base_url());
  SetupWebStoreGalleryUrl();
  SetupCrxDownloadAndUpdateUrls(embedded_test_server);
}

void FakeCWS::SetUpdateCrx(const std::string& app_id,
                           const std::string& crx_file,
                           const std::string& version) {
  GURL crx_download_url = web_store_url_.Resolve(kCrxDownloadPath + crx_file);

  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath crx_file_path =
      test_data_dir.AppendASCII("chromeos/app_mode/webstore/downloads")
          .AppendASCII(crx_file);
  std::string crx_content;
  ASSERT_TRUE(base::ReadFileToString(crx_file_path, &crx_content));

  const std::string sha256 = crypto::SHA256HashString(crx_content);
  const std::string sha256_hex = base::HexEncode(sha256.c_str(), sha256.size());

  SetUpdateCheckContent(
      "chromeos/app_mode/webstore/update_check/has_update.xml",
      crx_download_url,
      app_id,
      sha256_hex,
      base::UintToString(crx_content.size()),
      version,
      &update_check_content_);
}

void FakeCWS::SetNoUpdate(const std::string& app_id) {
  SetUpdateCheckContent("chromeos/app_mode/webstore/update_check/no_update.xml",
                        GURL(),
                        app_id,
                        "",
                        "",
                        "",
                        &update_check_content_);
}

void FakeCWS::SetupWebStore(const GURL& test_server_url) {
  std::string webstore_host(kWebstoreDomain);
  GURL::Replacements replace_webstore_host;
  replace_webstore_host.SetHostStr(webstore_host);
  web_store_url_ = test_server_url.ReplaceComponents(replace_webstore_host);
}

void FakeCWS::SetupWebStoreGalleryUrl() {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kAppsGalleryURL,
      web_store_url_.Resolve("/chromeos/app_mode/webstore").spec());
}

void FakeCWS::SetupCrxDownloadAndUpdateUrls(
    EmbeddedTestServer* embedded_test_server) {
  SetupCrxDownloadUrl();
  SetupCrxUpdateUrl(embedded_test_server);
}

void FakeCWS::SetupCrxDownloadUrl() {
  std::string downloads_path = std::string(kCrxDownloadPath).append("%s.crx");
  GURL downloads_url = web_store_url_.Resolve(downloads_path);
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kAppsGalleryDownloadURL, downloads_url.spec());
}

void FakeCWS::SetupCrxUpdateUrl(EmbeddedTestServer* embedded_test_server) {
  GURL update_url = web_store_url_.Resolve("/update_check.xml");
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ::switches::kAppsGalleryUpdateURL, update_url.spec());

  embedded_test_server->RegisterRequestHandler(
      base::Bind(&FakeCWS::HandleRequest, base::Unretained(this)));
}

void FakeCWS::SetUpdateCheckContent(const std::string& update_check_file,
                                    const GURL& crx_download_url,
                                    const std::string& app_id,
                                    const std::string& crx_fp,
                                    const std::string& crx_size,
                                    const std::string& version,
                                    std::string* update_check_content) {
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath update_file =
      test_data_dir.AppendASCII(update_check_file.c_str());
  ASSERT_TRUE(base::ReadFileToString(update_file, update_check_content));

  ReplaceSubstringsAfterOffset(update_check_content, 0, "$AppId", app_id);
  ReplaceSubstringsAfterOffset(
      update_check_content, 0, "$CrxDownloadUrl", crx_download_url.spec());
  ReplaceSubstringsAfterOffset(update_check_content, 0, "$FP", crx_fp);
  ReplaceSubstringsAfterOffset(update_check_content, 0, "$Size", crx_size);
  ReplaceSubstringsAfterOffset(update_check_content, 0, "$Version", version);
}

scoped_ptr<HttpResponse> FakeCWS::HandleRequest(const HttpRequest& request) {
  GURL request_url = GURL("http://localhost").Resolve(request.relative_url);
  std::string request_path = request_url.path();
  if (!update_check_content_.empty() &&
      request_path.find("/update_check.xml") != std::string::npos) {
    scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/xml");
    http_response->set_content(update_check_content_);
    return http_response.PassAs<HttpResponse>();
  }

  return scoped_ptr<HttpResponse>();
}

}  // namespace chromeos
