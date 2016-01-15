// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/browser/blimp_engine_config.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "blimp/engine/browser/switches.h"

namespace blimp {
namespace engine {

namespace {
// Gets the client token from the file provided by the command line. If a read
// does not succeed, or the switch is malformed, an empty string is returned.
std::string GetClientToken(const base::CommandLine& cmd_line) {
  std::string file_contents;
  const base::FilePath path = cmd_line.GetSwitchValuePath(kClientTokenPath);
  if (!base::ReadFileToString(path, &file_contents)) {
    LOG(ERROR) << "Could not read client token file at "
               << (path.empty() ? "(not provided)" : path.AsUTF8Unsafe());
  }
  return base::CollapseWhitespaceASCII(file_contents, true);
}
}  // namespace

BlimpEngineConfig::~BlimpEngineConfig() {}

// static
scoped_ptr<BlimpEngineConfig> BlimpEngineConfig::Create(
    const base::CommandLine& cmd_line) {
  const std::string client_token = GetClientToken(cmd_line);
  if (!client_token.empty()) {
    return make_scoped_ptr(new BlimpEngineConfig(client_token));
  }
  return nullptr;
}

// static
scoped_ptr<BlimpEngineConfig> BlimpEngineConfig::CreateForTest(
    const std::string& client_token) {
  return make_scoped_ptr(new BlimpEngineConfig(client_token));
}

const std::string& BlimpEngineConfig::client_token() const {
  return client_token_;
}

BlimpEngineConfig::BlimpEngineConfig(const std::string& client_token)
    : client_token_(client_token) {}

}  // namespace engine
}  // namespace blimp
