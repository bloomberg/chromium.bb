// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/page_state.h"

#include "base/files/file_path.h"
#include "base/logging.h"

namespace content {

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

}  // namespace content
