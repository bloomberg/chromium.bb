// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_OMNIBOX_OMNIBOX_APPLICATION_H_
#define MANDOLINE_UI_OMNIBOX_OMNIBOX_APPLICATION_H_

#include "base/macros.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/interface_factory.h"

namespace mojo {
class ApplicationImpl;
}

namespace mandoline {

class OmniboxApplication : public mojo::ApplicationDelegate,
                           public mojo::InterfaceFactory<Omnibox> {
 public:
  OmniboxApplication();
  ~OmniboxApplication() override;

 private:
  // Overridden from mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::InterfaceFactory<Omnibox>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Omnibox> request) override;

  mojo::ApplicationImpl* app_;
  mojo::TracingImpl tracing_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxApplication);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_OMNIBOX_OMNIBOX_APPLICATION_H_
