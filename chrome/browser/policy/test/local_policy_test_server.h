// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_
#define CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/test/local_test_server.h"

namespace crypto {
class RSAPrivateKey;
}

namespace policy {

// Runs a python implementation of the cloud policy server on the local machine.
class LocalPolicyTestServer : public net::LocalTestServer {
 public:
  // Initializes the test server to serve its policy from a temporary directory,
  // the contents of which can be updated via UpdatePolicy().
  LocalPolicyTestServer();

  // Initializes a test server configured by the configuration file
  // |config_file|.
  explicit LocalPolicyTestServer(const base::FilePath& config_file);

  // Initializes the test server with the configuration read from
  // chrome/test/data/policy/policy_|test_name|.json.
  explicit LocalPolicyTestServer(const std::string& test_name);

  virtual ~LocalPolicyTestServer();

  // Sets the policy signing key used by the server. This must be called before
  // starting the server, and only works when the server serves from a temporary
  // directory.
  bool SetSigningKey(const crypto::RSAPrivateKey* key);

  // Pre-configures a registered client so the server returns policy without the
  // client having to make a registration call. This must be called before
  // starting the server, and only works when the server serves from a temporary
  // directory.
  void RegisterClient(const std::string& dm_token,
                      const std::string& device_id);

  // Updates policy served by the server for a given (type, entity_id) pair.
  // This only works when the server serves from a temporary directory.
  //
  // |type| is the policy type as requested in the protocol via
  // |PolicyFetchRequest.policy_type|. |policy| is the payload data in the
  // format appropriate for |type|, which is usually a serialized protobuf (for
  // example, CloudPolicySettings or ChromeDeviceSettingsProto).
  bool UpdatePolicy(const std::string& type,
                    const std::string& entity_id,
                    const std::string& policy);

  // Gets the service URL.
  GURL GetServiceURL() const;

  // net::LocalTestServer:
  virtual bool SetPythonPath() const OVERRIDE;
  virtual bool GetTestServerPath(
      base::FilePath* testserver_path) const OVERRIDE;
  virtual bool GenerateAdditionalArguments(
      base::DictionaryValue* arguments) const OVERRIDE;

 private:
  base::FilePath config_file_;
  base::FilePath policy_key_;
  base::DictionaryValue clients_;
  base::ScopedTempDir server_data_dir_;

  DISALLOW_COPY_AND_ASSIGN(LocalPolicyTestServer);
};

}  // namespace

#endif  // CHROME_BROWSER_POLICY_TEST_LOCAL_POLICY_TEST_SERVER_H_
