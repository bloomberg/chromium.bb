// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/page_state.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace content {

// static
PageState PageState::CreateFromEncodedData(const std::string& data) {
  return PageState(data);
}

// static
PageState PageState::CreateFromURL(const GURL& url) {
  NOTIMPLEMENTED();
  return PageState();
}

// static
PageState PageState::CreateForTesting(
    const GURL& url,
    bool body_contains_password_data,
    const char* optional_body_data,
    const base::FilePath* optional_body_file_path) {
  NOTIMPLEMENTED();
  return PageState();
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

std::vector<base::FilePath> PageState::GetReferencedFiles() const {
  NOTIMPLEMENTED();
  return std::vector<base::FilePath>();
}

PageState PageState::RemovePasswordData() const {
  NOTIMPLEMENTED();
  return *this;
}

PageState PageState::RemoveScrollOffset() const {
  NOTIMPLEMENTED();
  return *this;
}

PageState::PageState(const std::string& data)
    : data_(data) {
  // TODO(darin): Enable this DCHECK once tests have been fixed up to not pass
  // bogus encoded data to CreateFromEncodedData.
  //DCHECK(IsValid());
}

}  // namespace content
