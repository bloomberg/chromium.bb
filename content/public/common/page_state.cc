// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/page_state.h"

namespace content {

// static
PageState PageState::CreateFromEncodedData(const std::string& data) {
  return PageState(data);
}

PageState::PageState() {
}

bool PageState::IsValid() const {
  return !data_.empty();
}

bool PageState::Equals(const PageState& other) const {
  return data_ == other.data_;
}

const std::string& PageState::ToEncodedData() const {
  return data_;
}

PageState::PageState(const std::string& data)
    : data_(data) {
  // TODO(darin): Enable this DCHECK once tests have been fixed up to not pass
  // bogus encoded data to CreateFromEncodedData.
  //DCHECK(IsValid());
}

}  // namespace content
