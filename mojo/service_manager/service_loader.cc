// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/service_manager/service_loader.h"

#include "base/logging.h"

namespace mojo {

ServiceLoader::SimpleLoadCallbacks::SimpleLoadCallbacks(
    ScopedMessagePipeHandle shell_handle)
    : shell_handle_(shell_handle.Pass()) {
}

ServiceLoader::SimpleLoadCallbacks::~SimpleLoadCallbacks() {
}

ScopedMessagePipeHandle
ServiceLoader::SimpleLoadCallbacks::RegisterApplication() {
  return shell_handle_.Pass();
}

void ServiceLoader::SimpleLoadCallbacks::LoadWithContentHandler(
    const GURL& content_handle_url, URLResponsePtr content) {
  NOTREACHED();
}

}  // namespace mojo
