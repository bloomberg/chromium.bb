// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include <base/strings/stringprintf.h>
#include "base/strings/string_util.h"

namespace autofill_assistant {

Details::Details(const ShowDetailsProto& proto) : proto_(proto) {
  // Legacy treatment for old proto fields. Can be removed once the backend
  // is updated to set the description_line1/line2 fields.
  if (details().has_description() && !details().has_description_line2()) {
    proto_.mutable_details()->set_description_line2(details().description());
  }
}

base::Value Details::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  if (!details().title().empty())
    dict.SetKey("title", base::Value(details().title()));

  if (!details().image_url().empty())
    dict.SetKey("image_url", base::Value(details().image_url()));

  if (!details().total_price().empty())
    dict.SetKey("total_price", base::Value(details().total_price()));

  if (!details().description_line1().empty())
    dict.SetKey("description_line1",
                base::Value(details().description_line1()));

  if (!details().description_line2().empty())
    dict.SetKey("description_line2",
                base::Value(details().description_line2()));

  if (details().has_datetime()) {
    dict.SetKey("datetime",
                base::Value(base::StringPrintf(
                    "%d-%02d-%02dT%02d:%02d:%02d",
                    static_cast<int>(details().datetime().date().year()),
                    details().datetime().date().month(),
                    details().datetime().date().day(),
                    details().datetime().time().hour(),
                    details().datetime().time().minute(),
                    details().datetime().time().second())));
  }
  if (!datetime_.empty())
    dict.SetKey("datetime_str", base::Value(datetime_));

  dict.SetKey("user_approval_required",
              base::Value(changes().user_approval_required()));
  dict.SetKey("highlight_title", base::Value(changes().highlight_title()));
  dict.SetKey("highlight_line1", base::Value(changes().highlight_line1()));
  dict.SetKey("highlight_line2", base::Value(changes().highlight_line2()));

  return dict;
}

bool Details::UpdateFromParameters(
    const std::map<std::string, std::string>& parameters) {
  bool is_updated = false;
  for (const auto& iter : parameters) {
    std::string key = iter.first;
    if (base::EndsWith(key, "MOVIE_NAME", base::CompareCase::SENSITIVE)) {
      proto_.mutable_details()->set_title(iter.second);
      is_updated = true;
      continue;
    }

    if (base::EndsWith(key, "DATETIME", base::CompareCase::SENSITIVE)) {
      // TODO(crbug.com/806868): Parse the string here and fill
      // proto.description_line1, then get rid of datetime_ in Details.
      datetime_ = iter.second;
      is_updated = true;
      continue;
    }

    if (base::EndsWith(key, "THEATER_NAME", base::CompareCase::SENSITIVE)) {
      proto_.mutable_details()->set_description_line2(iter.second);
      is_updated = true;
      continue;
    }
  }
  return is_updated;
}

void Details::ClearChanges() {
  proto_.clear_change_flags();
}

}  // namespace autofill_assistant
