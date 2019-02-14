// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_

#include "base/files/file_path.h"

namespace extensions {
class Extension;

namespace declarative_net_request {

// Holds paths for an extension ruleset.
struct RulesetSource {
  // Creates RulesetSource for |extension|. This must be called on a sequence
  // which supports file IO.
  static RulesetSource Create(const Extension& extension);

  ~RulesetSource();
  RulesetSource(RulesetSource&&);
  RulesetSource& operator=(RulesetSource&&);

  RulesetSource Clone() const;

  // Path to the JSON rules.
  base::FilePath json_path;

  // Path to the indexed flatbuffer rules.
  base::FilePath indexed_path;

 private:
  RulesetSource();

  DISALLOW_COPY_AND_ASSIGN(RulesetSource);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_SOURCE_H_
