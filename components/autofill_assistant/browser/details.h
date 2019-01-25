// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

struct Details {
  DetailsProto proto;
  DetailsChanges changes;

  // RFC 3339 date-time. Ignore if proto.datetime is set.
  //
  // TODO(crbug.com/806868): parse RFC 3339 date-time on the C++ side and fill
  // proto.datetime with the result instead of carrying a string representation
  // of the datetime.
  std::string datetime;

  // Returns a dictionary describing the current execution context, which
  // is intended to be serialized as JSON string. The execution context is
  // useful when analyzing feedback forms and for debugging in general.
  base::Value GetDebugContext() const;

  // Update details from the given parameters. Returns true if changes were
  // made.
  bool UpdateFromParameters(
      const std::map<std::string, std::string>& parameters);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
