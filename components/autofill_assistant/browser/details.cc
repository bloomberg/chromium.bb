// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include <base/strings/stringprintf.h>
#include "base/strings/string_util.h"

namespace autofill_assistant {

base::Value Details::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!proto.title().empty())
    dict.SetKey("title", base::Value(proto.title()));

  if (!proto.url().empty())
    dict.SetKey("url", base::Value(proto.url()));

  if (!proto.m_id().empty())
    dict.SetKey("mId", base::Value(proto.m_id()));

  if (!proto.total_price().empty())
    dict.SetKey("total_price", base::Value(proto.total_price()));

  if (!proto.description().empty())
    dict.SetKey("description", base::Value(proto.description()));

  if (proto.has_datetime()) {
    dict.SetKey(
        "datetime",
        base::Value(base::StringPrintf(
            "%d-%02d-%02dT%02d:%02d:%02d",
            static_cast<int>(proto.datetime().date().year()),
            proto.datetime().date().month(), proto.datetime().date().day(),
            proto.datetime().time().hour(), proto.datetime().time().minute(),
            proto.datetime().time().second())));
  }
  if (!datetime.empty())
    dict.SetKey("datetime_str", base::Value(datetime));

  dict.SetKey("user_approval_required",
              base::Value(changes.user_approval_required()));
  dict.SetKey("highlight_title", base::Value(changes.highlight_title()));
  dict.SetKey("highlight_date", base::Value(changes.highlight_date()));

  return dict;
}

bool Details::UpdateFromParameters(
    const std::map<std::string, std::string>& parameters) {
  bool is_updated = false;
  for (const auto& iter : parameters) {
    std::string key = iter.first;
    if (base::EndsWith(key, "E_NAME", base::CompareCase::SENSITIVE)) {
      proto.set_title(iter.second);
      is_updated = true;
      continue;
    }

    if (base::EndsWith(key, "R_NAME", base::CompareCase::SENSITIVE)) {
      proto.set_description(iter.second);
      is_updated = true;
      continue;
    }

    if (base::EndsWith(key, "MID", base::CompareCase::SENSITIVE)) {
      proto.set_m_id(iter.second);
      is_updated = true;
      continue;
    }

    if (base::EndsWith(key, "DATETIME", base::CompareCase::SENSITIVE)) {
      // TODO(crbug.com/806868): Parse the string here and fill proto.datetime,
      // then get rid of the field datetime in Details.
      datetime = iter.second;
      is_updated = true;
      continue;
    }
  }
  return is_updated;
}

}  // namespace autofill_assistant
