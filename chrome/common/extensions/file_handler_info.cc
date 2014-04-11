// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/file_handler_info.h"

namespace extensions {

FileHandlerInfo::FileHandlerInfo() {}
FileHandlerInfo::~FileHandlerInfo() {}

FileHandlersInfo::FileHandlersInfo(const std::vector<FileHandlerInfo>* handlers)
    : handlers(handlers) {}

FileHandlersInfo::~FileHandlersInfo() {}

}  // namespace extensions
