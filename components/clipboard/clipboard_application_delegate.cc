// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/clipboard/clipboard_application_delegate.h"

#include <utility>

#include "components/clipboard/clipboard_standalone_impl.h"
#include "mojo/shell/public/cpp/connection.h"

namespace clipboard {

ClipboardApplicationDelegate::ClipboardApplicationDelegate() {}

ClipboardApplicationDelegate::~ClipboardApplicationDelegate() {}

void ClipboardApplicationDelegate::Initialize(mojo::Shell* shell,
                                              const std::string& url,
                                              uint32_t id) {
  tracing_.Initialize(shell, url);
}

bool ClipboardApplicationDelegate::AcceptConnection(
    mojo::Connection* connection) {
  connection->AddInterface(this);
  return true;
}

void ClipboardApplicationDelegate::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<mojo::Clipboard> request) {
  // TODO(erg): Write native implementations of the clipboard. For now, we
  // just build a clipboard which doesn't interact with the system.
  new clipboard::ClipboardStandaloneImpl(std::move(request));
}

}  // namespace clipboard
