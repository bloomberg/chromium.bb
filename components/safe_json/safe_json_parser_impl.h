// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/safe_json/public/interfaces/safe_json.mojom.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace base {
class Value;
}

namespace safe_json {

class SafeJsonParserImpl : public SafeJsonParser {
 public:
  SafeJsonParserImpl(const std::string& unsafe_json,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback);

 private:
  ~SafeJsonParserImpl() override;

  // SafeJsonParser implementation.
  void Start() override;

  void StartOnIOThread();
  void OnConnectionError();

  // mojom::SafeJsonParser::Parse callback.
  void OnParseDone(std::unique_ptr<base::Value> result,
                   const base::Optional<std::string>& error);

  // Reports the result on the calling task runner via the |success_callback_|
  // or the |error_callback_|.
  void ReportResults(std::unique_ptr<base::Value> parsed_json,
                     const std::string& error);

  const std::string unsafe_json_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;

  std::unique_ptr<content::UtilityProcessMojoClient<mojom::SafeJsonParser>>
      mojo_json_parser_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserImpl);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
