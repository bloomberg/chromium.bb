// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
#define COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_

#include "components/dom_distiller/core/article_distillation_update.h"
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
  // If execute_callback is true, when DistillPage is called, a task will
  // immediately be posted to execute the callback with a simple
  // DistilledArticleProto.
  explicit FakeDistiller(bool execute_callback);
  virtual ~FakeDistiller();
  MOCK_METHOD0(Die, void());

  virtual void DistillPage(const GURL& url,
                           const DistillationFinishedCallback& article_callback,
                           const DistillationUpdateCallback& page_callback)
      OVERRIDE;

  void RunDistillerCallback(scoped_ptr<DistilledArticleProto> proto);

  GURL GetUrl() { return url_; }

  DistillationFinishedCallback GetArticleCallback() {
    return article_callback_;
  }

 private:
  void PostDistillerCallback(scoped_ptr<DistilledArticleProto> proto);
  void RunDistillerCallbackInternal(scoped_ptr<DistilledArticleProto> proto);

  bool execute_callback_;
  GURL url_;
  DistillationFinishedCallback article_callback_;
  DistillationUpdateCallback page_callback_;

  bool destruction_allowed_;
};

}  // namespace test
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_FAKE_DISTILLER_H_
