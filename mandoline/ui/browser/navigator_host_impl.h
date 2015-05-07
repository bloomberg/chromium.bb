// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_BROWSER_NAVIGATOR_HOST_IMPL_H_
#define MANDOLINE_UI_BROWSER_NAVIGATOR_HOST_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "mandoline/services/navigation/public/interfaces/navigation.mojom.h"
#include "mojo/common/weak_binding_set.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_request.h"

namespace mandoline {
class Browser;

class NavigatorHostImpl : public mojo::NavigatorHost {
 public:
  explicit NavigatorHostImpl(Browser* browser);
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

  Browser* browser_;
  mojo::WeakBindingSet<NavigatorHost> bindings_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorHostImpl);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_BROWSER_NAVIGATOR_HOST_IMPL_H_
