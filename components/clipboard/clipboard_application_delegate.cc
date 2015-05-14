// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/clipboard/clipboard_application_delegate.h"

#include "components/clipboard/clipboard_standalone_impl.h"
#include "mojo/application/public/cpp/application_connection.h"

namespace clipboard {

ClipboardApplicationDelegate::ClipboardApplicationDelegate() {}

ClipboardApplicationDelegate::~ClipboardApplicationDelegate() {}

bool ClipboardApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void ClipboardApplicationDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::Clipboard> request) {
  // TODO(erg): Write native implementations of the clipboard. For now, we
  // just build a clipboard which doesn't interact with the system.
  new clipboard::ClipboardStandaloneImpl(request.Pass());
}

}  // namespace clipboard
