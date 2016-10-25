// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/string_util.h"

#include "base/strings/string_util.h"
#include "components/ui_devtools/Protocol.h"

namespace ui {
namespace devtools {
namespace protocol {

std::unique_ptr<Value> parseJSON(const String& string) {
  DCHECK(base::IsStringUTF8(string));
  // TODO(mhashmi): 16-bit strings need to be handled
  return parseJSON(reinterpret_cast<const uint8_t*>(&string[0]),
                   string.length());
};

}  // namespace protocol
}  // namespace ws
}  // namespace ui
