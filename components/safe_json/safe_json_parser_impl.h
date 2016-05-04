// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
#define COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/safe_json/public/interfaces/safe_json.mojom.h"
#include "components/safe_json/safe_json_parser.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class ListValue;
class SequencedTaskRunner;
class Value;
}

namespace content {
class UtilityProcessHost;
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

  void ReportResults();
  void ReportResultsOnOriginThread();

  // Implementing pieces of the UtilityProcessHostClient interface.
  bool OnMessageReceived(const IPC::Message& message) override;

  // SafeJsonParser implementation.
  void Start() override;

  // mojom::SafeJsonParser::Parse callback.
  void OnParseDone(const base::ListValue& wrapper, mojo::String error);

  const std::string unsafe_json_;
  SuccessCallback success_callback_;
  ErrorCallback error_callback_;
  scoped_refptr<base::SequencedTaskRunner> caller_task_runner_;

  std::unique_ptr<base::Value> parsed_json_;
  std::string error_;

  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  mojom::SafeJsonParserPtr service_;

  // To ensure the UtilityProcessHost and Mojo service are only accessed on the
  // IO thread.
  base::ThreadChecker io_thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SafeJsonParserImpl);
};

}  // namespace safe_json

#endif  // COMPONENTS_SAFE_JSON_SAFE_JSON_PARSER_IMPL_H_
