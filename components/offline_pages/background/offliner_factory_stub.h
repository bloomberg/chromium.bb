// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_STUB_H_
#define COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_STUB_H_

#include <memory>

#include "components/offline_pages/background/offliner_factory.h"

namespace offline_pages {

class OfflinerStub;

// Test class stubbing out the functionality of OfflinerFactory.
// It is only used for test support.
class OfflinerFactoryStub : public OfflinerFactory {
 public:
  OfflinerFactoryStub();
  ~OfflinerFactoryStub() override;

  Offliner* GetOffliner(const OfflinerPolicy* policy) override;

 private:
  std::unique_ptr<OfflinerStub> offliner_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_BACKGROUND_OFFLINER_FACTORY_STUB_H_
