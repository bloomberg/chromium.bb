// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_CONTENT_HANDLER_IMPL_H_
#define MOJO_APPS_JS_CONTENT_HANDLER_IMPL_H_

#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"

namespace mojo {
namespace apps {

class ApplicationDelegateImpl;

// Starts a new JSApp for each OnConnect call().
class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  ContentHandlerImpl(ApplicationDelegateImpl* app_delegate_impl);

 private:
  virtual ~ContentHandlerImpl();
  virtual void OnConnect(
      const mojo::String& url,
      URLResponsePtr content,
      InterfaceRequest<ServiceProvider> service_provider) override;

  ApplicationDelegateImpl* app_delegate_impl_;
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_CONTENT_HANDLER_IMPL_H_
