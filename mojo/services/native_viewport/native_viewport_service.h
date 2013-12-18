// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_

#include "base/memory/scoped_vector.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojo/services/native_viewport/native_viewport_export.h"
#include "mojom/shell.h"

namespace mojo {

namespace shell {
class Context;
}

namespace services {

class NativeViewportService : public ShellClientStub {
  public:
   NativeViewportService(ScopedMessagePipeHandle shell_handle);
   virtual ~NativeViewportService();
   virtual void AcceptConnection(ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE;

   // For locally loaded services.
   void set_context(shell::Context* context) { context_ = context; }

  private:
   class NativeViewportImpl;
   RemotePtr<Shell> shell_;
   ScopedVector<NativeViewportImpl> viewports_;
   shell::Context* context_;
};

}  // namespace services
}  // namespace mojo

#if defined(OS_ANDROID)
MOJO_NATIVE_VIEWPORT_EXPORT mojo::services::NativeViewportService*
    CreateNativeViewportService(mojo::ScopedMessagePipeHandle shell_handle);
#endif

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
