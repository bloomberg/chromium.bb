// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_FAKE_CWS_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_FAKE_CWS_H_

#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace chromeos {

class FakeCWS {
 public:
  FakeCWS() {}
  virtual ~FakeCWS() {}

  void Init(net::test_server::EmbeddedTestServer* embedded_test_server);
  // Sets up the kiosk app update response.
  void SetUpdateCrx(const std::string& app_id,
                    const std::string& crx_file,
                    const std::string& version);
  void SetNoUpdate(const std::string& app_id);

 private:
  void SetupWebStore(const GURL& test_server_url);
  void SetupWebStoreGalleryUrl();
  void SetupCrxDownloadAndUpdateUrls(
      net::test_server::EmbeddedTestServer* embedded_test_server);
  void SetupCrxDownloadUrl();
  void SetupCrxUpdateUrl(
      net::test_server::EmbeddedTestServer* embedded_test_server);
  // Sets up |update_check_content_| used in update request response later by
  // kiosk app update server request handler |HandleRequest|.
  void SetUpdateCheckContent(const std::string& update_check_file,
                             const GURL& crx_download_url,
                             const std::string& app_id,
                             const std::string& crx_fp,
                             const std::string& crx_size,
                             const std::string& version,
                             std::string* update_check_content);
  // Request handler for kiosk app update server.
  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);

  GURL web_store_url_;
  std::string update_check_content_;

  DISALLOW_COPY_AND_ASSIGN(FakeCWS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_FAKE_CWS_H_
