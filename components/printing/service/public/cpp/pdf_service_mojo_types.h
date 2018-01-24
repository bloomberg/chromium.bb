// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_SERVICE_PUBLIC_CPP_PDF_SERVICE_MOJO_TYPES_H_
#define COMPONENTS_PRINTING_SERVICE_PUBLIC_CPP_PDF_SERVICE_MOJO_TYPES_H_

#include <unordered_map>

namespace printing {

// Create an alias for map<uint32, uint64> type.
using ContentToFrameMap = std::unordered_map<uint32_t, uint64_t>;

}  // namespace printing

#endif  // COMPONENTS_PRINTING_SERVICE_PUBLIC_CPP_PDF_SERVICE_MOJO_TYPES_H_
