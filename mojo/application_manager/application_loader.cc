// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application_manager/application_loader.h"

#include "base/logging.h"

namespace mojo {

ApplicationLoader::SimpleLoadCallbacks::SimpleLoadCallbacks(
    ScopedMessagePipeHandle shell_handle)
    : shell_handle_(shell_handle.Pass()) {
}

ApplicationLoader::SimpleLoadCallbacks::~SimpleLoadCallbacks() {
}

ScopedMessagePipeHandle
ApplicationLoader::SimpleLoadCallbacks::RegisterApplication() {
  return shell_handle_.Pass();
}

void ApplicationLoader::SimpleLoadCallbacks::LoadWithContentHandler(
    const GURL& content_handle_url,
    URLResponsePtr content) {
  NOTREACHED();
}

}  // namespace mojo
