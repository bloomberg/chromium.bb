// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_FRAME_SERVICES_H_
#define MANDOLINE_TAB_FRAME_SERVICES_H_

#include "base/basictypes.h"
#include "mandoline/tab/frame_user_data.h"
#include "mandoline/tab/public/interfaces/frame_tree.mojom.h"
#include "mojo/application/public/cpp/service_provider_impl.h"

namespace mandoline {

// FrameServices is a FrameUserData that manages the ServiceProviders exposed
// to each client. It is also responsible for obtaining the FrameTreeClient
// from the remote side.
class FrameServices : public FrameUserData {
 public:
  FrameServices();
  ~FrameServices() override;

  void Init(mojo::InterfaceRequest<mojo::ServiceProvider>* services,
            mojo::ServiceProviderPtr* exposed_services);

  FrameTreeClient* frame_tree_client() { return frame_tree_client_.get(); }

 private:
  mojo::ServiceProviderImpl exposed_services_;
  mojo::ServiceProviderPtr services_;
  FrameTreeClientPtr frame_tree_client_;

  DISALLOW_COPY_AND_ASSIGN(FrameServices);
};

}  // namespace mandoline

#endif  // MANDOLINE_TAB_FRAME_SERVICES_H_
