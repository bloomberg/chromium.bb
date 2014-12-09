// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/string_util.h"

#include "base/strings/string_util.h"

namespace web {

base::string16 GetStringByClippingLastWord(const base::string16& contents,
                                           size_t length) {
  if (contents.size() < length)
    return contents;

  base::string16 clipped_contents = contents.substr(0, length);
  size_t last_space_index =
      clipped_contents.find_last_of(base::kWhitespaceUTF16);
  // TODO(droger): Check if we should return the empty string instead.
  // See http://crbug.com/324304
  if (last_space_index != base::string16::npos)
    clipped_contents.resize(last_space_index);
  return clipped_contents;
}

}  // namespace web
