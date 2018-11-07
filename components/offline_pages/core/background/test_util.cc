// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

std::string SavePageRequest::ToString() const {
  base::DictionaryValue result;
  result.SetInteger("request_id", request_id_);
  result.SetString("url", url_.spec());
  result.SetString("client_id", client_id_.ToString());
  result.SetInteger("creation_time",
                    creation_time_.ToDeltaSinceWindowsEpoch().InSeconds());
  result.SetInteger("started_attempt_count", started_attempt_count_);
  result.SetInteger("completed_attempt_count", completed_attempt_count_);
  result.SetInteger("last_attempt_time",
                    last_attempt_time_.ToDeltaSinceWindowsEpoch().InSeconds());
  result.SetBoolean("user_requested", user_requested_);
  result.SetInteger("state", static_cast<int>(state_));
  result.SetInteger("fail_state", static_cast<int>(fail_state_));
  result.SetInteger("pending_state", static_cast<int>(pending_state_));
  result.SetString("original_url", original_url_.spec());
  result.SetString("request_origin", request_origin_);

  std::string result_string;
  base::JSONWriter::Write(result, &result_string);
  return result_string;
}

}  // namespace offline_pages
