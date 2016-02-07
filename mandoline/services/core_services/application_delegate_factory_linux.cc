// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/application_delegate_factory.h"

#include "components/font_service/font_service_app.h"

namespace core_services {

scoped_ptr<mojo::ShellClient> CreatePlatformSpecificApplicationDelegate(
    const std::string& url) {
  return url == "mojo://font_service/"
             ? make_scoped_ptr(new font_service::FontServiceApp)
             : nullptr;
}

}  // namespace core_services
