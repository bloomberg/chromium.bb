// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_
#define COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_

#include <memory>

#include "components/dom_distiller/core/distiller_page.h"

namespace dom_distiller {

class FaviconWebStateDispatcher;

// DistillerPageFactoryIOS is an iOS-specific implementation of the
// DistillerPageFactory interface allowing the creation of DistillerPage
// instances.
class DistillerPageFactoryIOS : public DistillerPageFactory {
 public:
  explicit DistillerPageFactoryIOS(
      std::unique_ptr<FaviconWebStateDispatcher> web_state_dispatcher);
  ~DistillerPageFactoryIOS() override;

  // Implementation of DistillerPageFactory:
  std::unique_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& view_size) const override;
  std::unique_ptr<DistillerPage> CreateDistillerPageWithHandle(
      std::unique_ptr<SourcePageHandle> handle) const override;

 private:
  std::unique_ptr<FaviconWebStateDispatcher> web_state_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(DistillerPageFactoryIOS);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_IOS_DISTILLER_PAGE_FACTORY_IOS_H_
