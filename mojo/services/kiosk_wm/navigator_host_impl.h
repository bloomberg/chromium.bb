// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_KIOSK_WM_NAVIGATOR_HOST_IMPL_H_
#define SERVICES_KIOSK_WM_NAVIGATOR_HOST_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mojo/common/weak_binding_set.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/mojo_services/src/navigation/public/interfaces/navigation.mojom.h"

namespace kiosk_wm {
class KioskWM;

class NavigatorHostImpl : public mojo::NavigatorHost {
 public:
  NavigatorHostImpl(KioskWM* kiosk_wm);
  ~NavigatorHostImpl() override;

  void Bind(mojo::InterfaceRequest<mojo::NavigatorHost> request);

  void RecordNavigation(const std::string& url);

  // mojo::NavigatorHost implementation:
  void DidNavigateLocally(const mojo::String& url) override;
  void RequestNavigate(mojo::Target target,
                       mojo::URLRequestPtr request) override;
  void RequestNavigateHistory(int32_t delta) override;

 private:
  std::vector<std::string> history_;
  int32_t current_index_;

  KioskWM* kiosk_wm_;
  mojo::WeakBindingSet<NavigatorHost> bindings_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorHostImpl);
};

}  // namespace kiosk_wm

#endif  // SERVICES_KIOSK_WM_NAVIGATOR_HOST_IMPL_H_
