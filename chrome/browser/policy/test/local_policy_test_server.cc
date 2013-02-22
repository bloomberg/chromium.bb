// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/test/local_policy_test_server.h"

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/policy/cloud_policy_constants.h"
#include "chrome/common/chrome_paths.h"
#include "crypto/rsa_private_key.h"
#include "net/test/base_test_server.h"
#include "net/test/python_utils.h"

namespace policy {

namespace {

// Filename in the temporary directory storing the policy data.
const base::FilePath::CharType kPolicyFileName[] = FILE_PATH_LITERAL("policy");

// Private signing key file within the temporary directory.
const base::FilePath::CharType kSigningKeyFileName[] =
    FILE_PATH_LITERAL("signing_key");

// The file containing client definitions to be passed to the server.
const base::FilePath::CharType kClientStateFileName[] =
    FILE_PATH_LITERAL("clients");

// Dictionary keys for the client state file. Needs to be kept in sync with
// policy_testserver.py.
const char kClientStateKeyAllowedPolicyTypes[] = "allowed_policy_types";
const char kClientStateKeyDeviceId[] = "device_id";
const char kClientStateKeyDeviceToken[] = "device_token";
const char kClientStateKeyMachineName[] = "machine_name";
const char kClientStateKeyMachineId[] = "machine_id";

// Checks whether a given character should be replaced when constructing a file
// name. To keep things simple, this is a bit over-aggressive. Needs to be kept
// in sync with policy_testserver.py.
bool IsUnsafeCharacter(char c) {
  return !(isalnum(c) || c == '.' || c == '@' || c == '-');
}

}  // namespace

LocalPolicyTestServer::LocalPolicyTestServer()
    : net::LocalTestServer(net::BaseTestServer::TYPE_HTTP,
                           net::BaseTestServer::kLocalhost,
                           base::FilePath()) {
  CHECK(server_data_dir_.CreateUniqueTempDir());
  config_file_ = server_data_dir_.path().Append(kPolicyFileName);
}

LocalPolicyTestServer::LocalPolicyTestServer(const base::FilePath& config_file)
    : net::LocalTestServer(net::BaseTestServer::TYPE_HTTP,
                           net::BaseTestServer::kLocalhost,
                           base::FilePath()),
      config_file_(config_file) {}

LocalPolicyTestServer::LocalPolicyTestServer(const std::string& test_name)
    : net::LocalTestServer(net::BaseTestServer::TYPE_HTTP,
                           net::BaseTestServer::kLocalhost,
                           base::FilePath()) {
  // Read configuration from a file in chrome/test/data/policy.
  base::FilePath source_root;
  CHECK(PathService::Get(chrome::DIR_TEST_DATA, &source_root));
  config_file_ = source_root
      .AppendASCII("policy")
      .AppendASCII(base::StringPrintf("policy_%s.json", test_name.c_str()));
}

LocalPolicyTestServer::~LocalPolicyTestServer() {}

bool LocalPolicyTestServer::SetSigningKey(const crypto::RSAPrivateKey* key) {
  CHECK(server_data_dir_.IsValid());

  std::vector<uint8> signing_key_bits;
  if (!key->ExportPrivateKey(&signing_key_bits))
    return false;

  policy_key_ = server_data_dir_.path().Append(kSigningKeyFileName);
  int bytes_written = file_util::WriteFile(
      policy_key_,
      reinterpret_cast<const char*>(vector_as_array(&signing_key_bits)),
      signing_key_bits.size());
  return bytes_written == static_cast<int>(signing_key_bits.size());
}

void LocalPolicyTestServer::RegisterClient(const std::string& dm_token,
                                           const std::string& device_id) {
  CHECK(server_data_dir_.IsValid());

  scoped_ptr<base::DictionaryValue> client_dict(new base::DictionaryValue());
  client_dict->SetString(kClientStateKeyDeviceId, device_id);
  client_dict->SetString(kClientStateKeyDeviceToken, dm_token);
  client_dict->SetString(kClientStateKeyMachineName, std::string());
  client_dict->SetString(kClientStateKeyMachineId, std::string());

  // Allow all policy types for now.
  scoped_ptr<base::ListValue> types(new base::ListValue());
  types->AppendString(dm_protocol::kChromeDevicePolicyType);
  types->AppendString(dm_protocol::kChromeUserPolicyType);
  types->AppendString(dm_protocol::kChromePublicAccountPolicyType);
  types->AppendString(dm_protocol::kChromeExtensionPolicyType);

  client_dict->Set(kClientStateKeyAllowedPolicyTypes, types.release());
  clients_.Set(dm_token, client_dict.release());
}

bool LocalPolicyTestServer::UpdatePolicy(const std::string& type,
                                         const std::string& entity_id,
                                         const std::string& policy) {
  CHECK(server_data_dir_.IsValid());

  std::string selector = type;
  if (!entity_id.empty())
    selector = base::StringPrintf("%s/%s", type.c_str(), entity_id.c_str());
  std::replace_if(selector.begin(), selector.end(), IsUnsafeCharacter, '_');

  base::FilePath policy_file = server_data_dir_.path().AppendASCII(
      base::StringPrintf("policy_%s.bin", selector.c_str()));

  return file_util::WriteFile(policy_file, policy.c_str(), policy.size()) ==
      static_cast<int>(policy.size());
}

GURL LocalPolicyTestServer::GetServiceURL() const {
  return GetURL("device_management");
}

bool LocalPolicyTestServer::SetPythonPath() const {
  if (!net::LocalTestServer::SetPythonPath())
    return false;

  // Add the net/tools/testserver directory to the path.
  base::FilePath net_testserver_path;
  if (!LocalTestServer::GetTestServerPath(&net_testserver_path)) {
    LOG(ERROR) << "Failed to get net testserver path.";
    return false;
  }
  AppendToPythonPath(net_testserver_path.DirName());

  // We need protobuf python bindings.
  base::FilePath third_party_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &third_party_dir)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  AppendToPythonPath(third_party_dir
                     .AppendASCII("third_party")
                     .AppendASCII("protobuf")
                     .AppendASCII("python"));

  // Add the generated python protocol buffer bindings.
  base::FilePath pyproto_dir;
  if (!GetPyProtoPath(&pyproto_dir)) {
    LOG(ERROR) << "Cannot find pyproto dir for generated code.";
    return false;
  }

  AppendToPythonPath(pyproto_dir
                     .AppendASCII("chrome")
                     .AppendASCII("browser")
                     .AppendASCII("policy")
                     .AppendASCII("proto"));
  return true;
}

bool LocalPolicyTestServer::GetTestServerPath(
    base::FilePath* testserver_path) const {
  base::FilePath source_root;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &source_root)) {
    LOG(ERROR) << "Failed to get DIR_SOURCE_ROOT";
    return false;
  }
  *testserver_path = source_root
      .AppendASCII("chrome")
      .AppendASCII("browser")
      .AppendASCII("policy")
      .AppendASCII("test")
      .AppendASCII("policy_testserver.py");
  return true;
}

bool LocalPolicyTestServer::GenerateAdditionalArguments(
    base::DictionaryValue* arguments) const {
  if (!net::LocalTestServer::GenerateAdditionalArguments(arguments))
    return false;

  arguments->SetString("config-file", config_file_.AsUTF8Unsafe());
  if (!policy_key_.empty())
    arguments->SetString("policy-key", policy_key_.AsUTF8Unsafe());
  if (server_data_dir_.IsValid()) {
    arguments->SetString("data-dir", server_data_dir_.path().AsUTF8Unsafe());

    if (!clients_.empty()) {
      std::string json;
      base::JSONWriter::Write(&clients_, &json);
      base::FilePath client_state_file =
          server_data_dir_.path().Append(kClientStateFileName);
      if (file_util::WriteFile(client_state_file, json.c_str(), json.size()) !=
          static_cast<int>(json.size())) {
        return false;
      }
      arguments->SetString("client-state", client_state_file.AsUTF8Unsafe());
    }
  }

  return true;
}

}  // namespace policy;
