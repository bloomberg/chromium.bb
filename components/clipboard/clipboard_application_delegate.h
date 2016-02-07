// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLIPBOARD_CLIBPOARD_APPLICATION_DELEGATE_H_
#define COMPONENTS_CLIPBOARD_CLIBPOARD_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "components/clipboard/public/interfaces/clipboard.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
class ApplicationConnection;
}

namespace clipboard {

class ClipboardApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::Clipboard> {
 public:
  ClipboardApplicationDelegate();
  ~ClipboardApplicationDelegate() override;

  // mojo::ApplicationDelegate implementation.
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override;
  bool AcceptConnection(
      mojo::ApplicationConnection* connection) override;

  // mojo::InterfaceFactory<mojo::Clipboard> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Clipboard> request) override;

 private:
  mojo::TracingImpl tracing_;

  DISALLOW_COPY_AND_ASSIGN(ClipboardApplicationDelegate);
};

}  // namespace clipboard

#endif  // COMPONENTS_CLIPBOARD_CLIBPOARD_APPLICATION_DELEGATE_H_
