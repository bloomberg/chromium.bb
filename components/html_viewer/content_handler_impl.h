// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_CONTENT_HANDLER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_CONTENT_HANDLER_IMPL_H_

#include "base/macros.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace html_viewer {

class GlobalState;

class ContentHandlerImpl : public mojo::ContentHandler {
 public:
  ContentHandlerImpl(GlobalState* global_state,
                     mojo::ApplicationImpl* app,
                     mojo::InterfaceRequest<ContentHandler> request);
  ~ContentHandlerImpl() override;

 protected:
  GlobalState* global_state() const { return global_state_; }
  mojo::ApplicationImpl* app() const { return app_; }

 private:
  // Overridden from ContentHandler:
  void StartApplication(
      mojo::InterfaceRequest<mojo::Application> request,
      mojo::URLResponsePtr response,
      const mojo::Callback<void()>& destruct_callback) override;

  GlobalState* global_state_;
  mojo::ApplicationImpl* app_;
  mojo::StrongBinding<mojo::ContentHandler> binding_;
  scoped_ptr<mojo::AppRefCount> app_refcount_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_CONTENT_HANDLER_IMPL_H_
