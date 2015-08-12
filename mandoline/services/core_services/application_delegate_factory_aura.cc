// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/application_delegate_factory.h"

#include "mandoline/ui/omnibox/omnibox_impl.h"

namespace core_services {

scoped_ptr<mojo::ApplicationDelegate> CreateApplicationDelegateAura(
    const std::string& url) {
  return url == "mojo://omnibox/" ? make_scoped_ptr(new mandoline::OmniboxImpl)
                                  : nullptr;
}

}  // namespace core_services
