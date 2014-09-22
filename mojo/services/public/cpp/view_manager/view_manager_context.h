// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CONTEXT_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CONTEXT_H_

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/service_provider_impl.h"

namespace mojo {
class ApplicationImpl;

class ViewManagerContext {
 public:
  explicit ViewManagerContext(ApplicationImpl* application_impl);
  ~ViewManagerContext();

  // Embed an application @ |url| at an appropriate View.
  // The first time this method is called in the life time of the View Manager
  // service the "appropriate View" is defined as being the service' root View.
  // Subsequent times, the implementation of this method is delegated to the
  // application embedded at the service root View. This application will have a
  // specific definition of where within its View hierarchy to embed an
  // un-parented URL. |exported_services| encapsulates services offered by the
  // application calling Embed() to the application being embedded. Returns
  // an object implementing ServiceProvider encapsulating services offered by
  // the embedded application to the embedder.
  void Embed(const String& url);
  scoped_ptr<ServiceProvider> Embed(
      const String& url,
      scoped_ptr<ServiceProviderImpl> exported_services);

 private:
  class InternalState;
  scoped_ptr<InternalState> state_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ViewManagerContext);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_CONTEXT_H_
