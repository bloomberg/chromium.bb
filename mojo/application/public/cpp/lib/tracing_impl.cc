// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/lib/tracing_impl.h"

#include "base/trace_event/trace_event_impl.h"
#include "mojo/application/public/cpp/application_impl.h"

#ifdef NDEBUG
#include "base/command_line.h"
#include "mojo/application/public/cpp/switches.h"
#endif

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

#ifdef NDEBUG
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEarlyTracing)) {
    provider_impl_.ForceEnableTracing();
  }
#else
  provider_impl_.ForceEnableTracing();
#endif
}

void TracingImpl::Create(ApplicationConnection* connection,
                         InterfaceRequest<tracing::TraceProvider> request) {
  provider_impl_.Bind(request.Pass());
}

}  // namespace mojo
