// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCOPED_UI_RESOURCE_H_
#define CC_TEST_FAKE_SCOPED_UI_RESOURCE_H_

#include "base/memory/ref_counted.h"
#include "cc/resources/scoped_ui_resource.h"

namespace cc {

class FakeScopedUIResource : public ScopedUIResource {
 public:
  static std::unique_ptr<FakeScopedUIResource> Create(
      UIResourceManager* ui_resource_manager);

  UIResourceBitmap GetBitmap(UIResourceId uid, bool resource_lost) override;
  void DeleteResource();
  void ResetCounters();

  int resource_create_count;
  int lost_resource_count;

 private:
  explicit FakeScopedUIResource(UIResourceManager* ui_resource_manager);
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCOPED_UI_RESOURCE_H_
