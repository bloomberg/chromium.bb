// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/ios/distiller_page_factory_ios.h"

#include "base/memory/ptr_util.h"
#include "components/dom_distiller/ios/distiller_page_ios.h"
#include "components/dom_distiller/ios/favicon_web_state_dispatcher.h"
#include "ios/web/public/browser_state.h"

namespace dom_distiller {

DistillerPageFactoryIOS::DistillerPageFactoryIOS(
    std::unique_ptr<FaviconWebStateDispatcher> web_state_dispatcher)
    : web_state_dispatcher_(std::move(web_state_dispatcher)) {}

DistillerPageFactoryIOS::~DistillerPageFactoryIOS() {}

std::unique_ptr<DistillerPage> DistillerPageFactoryIOS::CreateDistillerPage(
    const gfx::Size& view_size) const {
  return base::MakeUnique<DistillerPageIOS>(web_state_dispatcher_.get());
}

std::unique_ptr<DistillerPage>
DistillerPageFactoryIOS::CreateDistillerPageWithHandle(
    std::unique_ptr<SourcePageHandle> handle) const {
  return base::MakeUnique<DistillerPageIOS>(web_state_dispatcher_.get());
}

}  // namespace dom_distiller
