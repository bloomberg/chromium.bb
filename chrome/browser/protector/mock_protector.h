// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_H_
#define CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_H_
#pragma once

#include "chrome/browser/protector/protector.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace protector {

class MockProtector : public Protector {
 public:
  explicit MockProtector(Profile* profile);
  virtual ~MockProtector();

  MOCK_METHOD1(ShowChange, void(BaseSettingChange*));
  MOCK_METHOD0(DismissChange, void());
  MOCK_METHOD1(OpenTab, void(const GURL&));

  MOCK_METHOD0(OnApplyChange, void());
  MOCK_METHOD0(OnDiscardChange, void());
  MOCK_METHOD0(OnDecisionTimeout, void());
  MOCK_METHOD0(OnRemovedFromProfile, void());
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_H_
