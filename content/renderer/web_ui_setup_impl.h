// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_UI_SETUP_IMPL_H_
#define CONTENT_RENDERER_WEB_UI_SETUP_IMPL_H_

#include "base/basictypes.h"
#include "content/common/web_ui_setup.mojom.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/remote_ptr.h"

namespace content {

class WebUISetupImpl : public WebUISetup,
                       public mojo::ErrorHandler {
 public:
  static void Bind(mojo::ScopedMessagePipeHandle handle);

 private:
  explicit WebUISetupImpl(mojo::ScopedMessagePipeHandle handle);
  virtual ~WebUISetupImpl();

  // WebUISetup methods:
  virtual void SetWebUIHandle(
      int32_t view_routing_id,
      mojo::ScopedMessagePipeHandle web_ui_handle) OVERRIDE;

  // mojo::ErrorHandler methods:
  virtual void OnError() OVERRIDE;

  mojo::RemotePtr<WebUISetupClient> client_;

  DISALLOW_COPY_AND_ASSIGN(WebUISetupImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_UI_SETUP_IMPL_H_
