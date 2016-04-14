// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_MOJO_APP_CONNECTION_IMPL_H_
#define CONTENT_BROWSER_MOJO_MOJO_APP_CONNECTION_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/mojo_app_connection.h"
#include "services/shell/public/interfaces/interface_provider.mojom.h"

namespace content {

// Implementation of the app connection mechanism provided to browser code.
class MojoAppConnectionImpl : public MojoAppConnection {
 public:
  // Takes a BrowserContext and derives a mojo userid from it for this
  // connection.
  MojoAppConnectionImpl(const std::string& user_id,
                        const std::string& name,
                        const std::string& requestor_name);
  ~MojoAppConnectionImpl() override;

 private:
  // MojoAppConnection:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle handle) override;

  shell::mojom::InterfaceProviderPtr interfaces_;

  DISALLOW_COPY_AND_ASSIGN(MojoAppConnectionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_MOJO_APP_CONNECTION_IMPL_H_
