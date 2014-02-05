// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_

#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/distiller.h"
#include "testing/gmock/include/gmock/gmock.h"
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
  explicit FakeDistiller(bool execute_callback);
  virtual ~FakeDistiller();
  MOCK_METHOD0(Die, void());

  virtual void DistillPage(const GURL& url,
                           const DistillerCallback& callback) OVERRIDE;

  void RunDistillerCallback(scoped_ptr<DistilledArticleProto> proto);

  GURL GetUrl() { return url_; }

  DistillerCallback GetCallback() { return callback_; }

 private:
  void RunDistillerCallbackInternal(scoped_ptr<DistilledArticleProto> proto);

  bool execute_callback_;
  GURL url_;
  DistillerCallback callback_;
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
