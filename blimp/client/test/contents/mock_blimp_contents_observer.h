// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_TEST_CONTENTS_MOCK_BLIMP_CONTENTS_OBSERVER_H_
#define BLIMP_CLIENT_TEST_CONTENTS_MOCK_BLIMP_CONTENTS_OBSERVER_H_

#include "blimp/client/public/contents/blimp_contents_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blimp {
namespace client {

class MockBlimpContentsObserver : public BlimpContentsObserver {
 public:
  explicit MockBlimpContentsObserver(BlimpContents* contents);
  ~MockBlimpContentsObserver() override;

  MOCK_METHOD0(OnNavigationStateChanged, void());
  MOCK_METHOD1(OnLoadingStateChanged, void(bool loading));
  MOCK_METHOD1(OnPageLoadingStateChanged, void(bool loading));
  MOCK_METHOD0(OnContentsDestroyed, void());
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_TEST_CONTENTS_MOCK_BLIMP_CONTENTS_OBSERVER_H_
