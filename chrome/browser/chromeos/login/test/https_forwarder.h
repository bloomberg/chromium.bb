// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/test/spawned_test_server/local_test_server.h"
#include "url/gurl.h"

namespace chromeos {

// An https test server that forwards all requests to another server. This
// allows a server that supports http only to be accessed over https.
class HTTPSForwarder : public net::LocalTestServer {
 public:
  explicit HTTPSForwarder(const GURL& forward_target);
  virtual ~HTTPSForwarder();

  // net::LocalTestServer:
  virtual bool SetPythonPath() const OVERRIDE;
  virtual bool GetTestServerPath(
      base::FilePath* testserver_path) const OVERRIDE;
  virtual bool GenerateAdditionalArguments(
      base::DictionaryValue* arguments) const OVERRIDE;

 private:
  GURL forward_target_;

  DISALLOW_COPY_AND_ASSIGN(HTTPSForwarder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HTTPS_FORWARDER_H_
