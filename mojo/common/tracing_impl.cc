// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/tracing_impl.h"

#include "base/trace_event/trace_event.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/trace_controller_impl.h"

namespace mojo {

TracingImpl::TracingImpl() {
}

TracingImpl::~TracingImpl() {
}

void TracingImpl::Initialize(ApplicationImpl* app) {
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = mojo::String::From("mojo:tracing");
  connection_ = app->ConnectToApplication(request.Pass());
  connection_->AddService(this);
}

void TracingImpl::Create(ApplicationConnection* connection,
                         InterfaceRequest<tracing::TraceController> request) {
  new TraceControllerImpl(request.Pass());
}

}  // namespace mojo
