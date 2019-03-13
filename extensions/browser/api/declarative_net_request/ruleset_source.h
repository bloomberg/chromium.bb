// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"

namespace base {
class Token;
}  // namespace base

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace extensions {
class Extension;
struct InstallWarning;

namespace declarative_net_request {

struct IndexAndPersistRulesResult {
 public:
  static IndexAndPersistRulesResult CreateSuccessResult(
      int ruleset_checksum,
      std::vector<InstallWarning> warnings);
  static IndexAndPersistRulesResult CreateErrorResult(std::string error);

  ~IndexAndPersistRulesResult();
  IndexAndPersistRulesResult(IndexAndPersistRulesResult&&);
  IndexAndPersistRulesResult& operator=(IndexAndPersistRulesResult&&);

  // Whether IndexAndPersistRules succeeded.
  bool success;

  // Checksum of the persisted indexed ruleset file. Valid if |success| if true.
  int ruleset_checksum;

  // Valid if |success| is true.
  std::vector<InstallWarning> warnings;

  // Valid if |success| is false.
  std::string error;

 private:
  IndexAndPersistRulesResult();
  DISALLOW_COPY_AND_ASSIGN(IndexAndPersistRulesResult);
};

// Holds paths for an extension ruleset.
class RulesetSource {
 public:
  // This must only be called for extensions which specified a declarative
  // ruleset.
  static RulesetSource Create(const Extension& extension);

  RulesetSource(base::FilePath json_path, base::FilePath indexed_path);
  ~RulesetSource();
  RulesetSource(RulesetSource&&);
  RulesetSource& operator=(RulesetSource&&);

  RulesetSource Clone() const;

  // Path to the JSON rules.
  const base::FilePath& json_path() const { return json_path_; }

  // Path to the indexed flatbuffer rules.
  const base::FilePath& indexed_path() const { return indexed_path_; }

  // Each ruleset source within an extension has a distinct ID and priority.
  size_t id() const { return id_; }
  size_t priority() const { return priority_; }

  // Indexes and persists the JSON ruleset. This is potentially unsafe since the
  // JSON rules file is parsed in-process. Note: This must be called on a
  // sequence where file IO is allowed.
  IndexAndPersistRulesResult IndexAndPersistRulesUnsafe() const;

  using IndexAndPersistRulesCallback =
      base::OnceCallback<void(IndexAndPersistRulesResult)>;
  // Same as IndexAndPersistRulesUnsafe but parses the JSON rules file out-of-
  // process. |connector| should be a connector to the ServiceManager usable on
  // the current sequence. Optionally clients can pass a valid
  // |decoder_batch_id| to be used when accessing the data decoder service,
  // which is used internally to parse JSON.
  //
  // NOTE: This must be called on a sequence where file IO is allowed.
  void IndexAndPersistRules(service_manager::Connector* connector,
                            const base::Optional<base::Token>& decoder_batch_id,
                            IndexAndPersistRulesCallback callback) const;

 private:
  base::FilePath json_path_;
  base::FilePath indexed_path_;
  size_t id_;
  size_t priority_;

  DISALLOW_COPY_AND_ASSIGN(RulesetSource);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
