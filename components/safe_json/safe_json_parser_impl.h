// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class ListValue;
class SequencedTaskRunner;
class Value;
}

namespace IPC {
class Message;
}

namespace safe_json {

class SafeJsonParserImpl : public content::UtilityProcessHostClient,
                           public SafeJsonParser {
 public:
  SafeJsonParserImpl(const std::string& unsafe_json,
                     const SuccessCallback& success_callback,
                     const ErrorCallback& error_callback);

 private:
  ~SafeJsonParserImpl() override;

  void StartWorkOnIOThread();

  void OnJSONParseSucceeded(const base::ListValue& wrapper);
  void OnJSONParseFailed(const std::string& error_message);

  void ReportResults();
  void ReportResultsOnOriginThread();

  // Implementing pieces of the UtilityProcessHostClient interface.
  bool OnMessageReceived(const IPC::Message& message) override;

  // SafeJsonParser implementation.
  void Start() override;

  const std::string unsafe_json_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;

  std::unique_ptr<base::Value> parsed_json_;
  std::string error_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserImpl);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
