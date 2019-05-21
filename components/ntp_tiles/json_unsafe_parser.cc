// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/json_unsafe_parser.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_parser.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"

namespace ntp_tiles {

void JsonUnsafeParser::Parse(const std::string& unsafe_json,
                             const SuccessCallback& success_callback,
                             const ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const std::string& unsafe_json,
             const SuccessCallback& success_callback,
             const ErrorCallback& error_callback) {
            base::JSONReader::ValueWithError value_with_error =
                base::JSONReader::ReadAndReturnValueWithError(
                    unsafe_json, base::JSON_ALLOW_TRAILING_COMMAS);
            if (value_with_error.value) {
              success_callback.Run(std::move(*value_with_error.value));
            } else {
              error_callback.Run(base::StringPrintf(
                  "%s (%d:%d)", value_with_error.error_message.c_str(),
                  value_with_error.error_line, value_with_error.error_column));
            }
          },
          unsafe_json, success_callback, error_callback));
}

}  // namespace ntp_tiles
