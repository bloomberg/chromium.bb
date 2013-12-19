// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_

#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class GURL;

namespace dom_distiller {
namespace test {

class MockDistillerFactory : public DistillerFactory {
 public:
  MockDistillerFactory();
  virtual ~MockDistillerFactory();
  MOCK_METHOD0(CreateDistillerImpl, Distiller*());
  virtual scoped_ptr<Distiller> CreateDistiller() OVERRIDE {
    return scoped_ptr<Distiller>(CreateDistillerImpl());
  }
};

class FakeDistiller : public Distiller {
 public:
  FakeDistiller();
  virtual ~FakeDistiller();
  MOCK_METHOD0(Die, void());

  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) OVERRIDE {
    url_ = url;
    callback_ = callback;
  }

  void RunDistillerCallback(scoped_ptr<DistilledPageProto> proto) {
    EXPECT_FALSE(callback_.is_null());
    callback_.Run(proto.Pass());
    callback_.Reset();
  }

  GURL GetUrl() { return url_; }

  DistillerCallback GetCallback() { return callback_; }

 private:
  GURL url_;
  DistillerCallback callback_;
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
