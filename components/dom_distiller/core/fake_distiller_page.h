// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_

#include "components/dom_distiller/core/distiller_page.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace dom_distiller {
namespace test {

class MockDistillerPageFactory : public DistillerPageFactory {
 public:
  MockDistillerPageFactory();
  virtual ~MockDistillerPageFactory();
  MOCK_CONST_METHOD0(CreateDistillerPageImpl, DistillerPage*());
  virtual scoped_ptr<DistillerPage> CreateDistillerPage(
      const gfx::Size& render_view_size) const OVERRIDE {
    return scoped_ptr<DistillerPage>(CreateDistillerPageImpl());
  }
  virtual scoped_ptr<DistillerPage> CreateDistillerPageWithHandle(
      scoped_ptr<SourcePageHandle> handle) const OVERRIDE {
    return scoped_ptr<DistillerPage>(CreateDistillerPageImpl());
  }
};

class MockDistillerPage : public DistillerPage {
 public:
  MockDistillerPage();
  virtual ~MockDistillerPage();
  MOCK_METHOD2(DistillPageImpl,
               void(const GURL& gurl, const std::string& script));
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_PAGE_H_
