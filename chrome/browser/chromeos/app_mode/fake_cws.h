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

// Simple fake CWS update check request handler that returns a fixed update
// check response. The response is created either from SetUpdateCrx() or
// SetNoUpdate().
class FakeCWS {
 public:
  FakeCWS();
  ~FakeCWS();

  // Initializes as CWS request handler and overrides app gallery command line
  // switches.
  void Init(net::test_server::EmbeddedTestServer* embedded_test_server);

  // Initializes as a private store handler using the given server and URL end
  // point. Private store does not override app gallery command lines and use a
  // slightly different template (as documented on
  // https://developer.chrome.com/extensions/autoupdate).
  void InitAsPrivateStore(
      net::test_server::EmbeddedTestServer* embedded_test_server,
      const std::string& update_check_end_point);

  // Sets up the update check response with has_update template.
  void SetUpdateCrx(const std::string& app_id,
                    const std::string& crx_file,
                    const std::string& version);

  // Sets up the update check response with no_update template.
  void SetNoUpdate(const std::string& app_id);

  // Returns the current |update_check_count_| and resets it.
  int GetUpdateCheckCountAndReset();

 private:
  void SetupWebStoreURL(const GURL& test_server_url);
  void OverrideGalleryCommandlineSwitches();

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

  std::string has_update_template_;
  std::string no_update_template_;
  std::string update_check_end_point_;

  std::string update_check_content_;
  int update_check_count_;

  DISALLOW_COPY_AND_ASSIGN(FakeCWS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_FAKE_CWS_H_
