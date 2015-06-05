// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MOJO_APP_CONNECTION_H_
#define CONTENT_PUBLIC_BROWSER_MOJO_APP_CONNECTION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_ptr.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo/src/mojo/public/cpp/system/message_pipe.h"

class GURL;

namespace content {

// This provides a way for arbitrary browser code to connect to Mojo apps. These
// objects are not thread-safe but may be constructed and used on any single
// thread.
class CONTENT_EXPORT MojoAppConnection {
 public:
  virtual ~MojoAppConnection() {}

  // Creates a new connection to the application at |url|. This may be called
  // from any thread.
  static scoped_ptr<MojoAppConnection> Create(const GURL& url);

  // Connects to a service within the application.
  template <typename Interface>
  void ConnectToService(mojo::InterfacePtr<Interface>* proxy) {
    ConnectToService(Interface::Name_, mojo::GetProxy(proxy).PassMessagePipe());
  }

  virtual void ConnectToService(const std::string& service_name,
                                mojo::ScopedMessagePipeHandle handle) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MOJO_APP_CONNECTION_H_
