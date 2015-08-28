// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/application_delegate_factory.h"

#include "mandoline/ui/desktop_ui/browser_manager.h"
#include "mandoline/ui/omnibox/omnibox_application.h"

namespace core_services {

scoped_ptr<mojo::ApplicationDelegate> CreateApplicationDelegateAura(
    const std::string& url) {
  if (url == "mojo://desktop_ui/")
    return make_scoped_ptr(new mandoline::BrowserManager);
  else if (url == "mojo://omnibox/")
    return make_scoped_ptr(new mandoline::OmniboxApplication);
  return nullptr;
}

}  // namespace core_services
