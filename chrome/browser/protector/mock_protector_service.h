// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_SERVICE_H_
#define CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_SERVICE_H_
#pragma once

#include "chrome/browser/protector/protector_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace protector {

class MockProtectorService : public ProtectorService {
 public:
  // Creates and returns the MockProtectorService instance associated with
  // |profile|. Should be called before any calls to
  // ProtectorServiceFactory::GetForProfile are made, otherwise a (non-mocked)
  // ProtectorService instance will be associated with |profile|.
  static MockProtectorService* BuildForProfile(Profile* profile);

  explicit MockProtectorService(Profile* profile);
  virtual ~MockProtectorService();

  MOCK_METHOD1(ShowChange, void(BaseSettingChange*));
  MOCK_CONST_METHOD0(IsShowingChange, bool());

  MOCK_METHOD0(DismissChange, void());
  MOCK_METHOD0(ApplyChange, void());
  MOCK_METHOD0(DiscardChange, void());

  MOCK_METHOD1(OpenTab, void(const GURL&));

  MOCK_METHOD0(OnApplyChange, void());
  MOCK_METHOD0(OnDiscardChange, void());
  MOCK_METHOD0(OnDecisionTimeout, void());
  MOCK_METHOD0(OnRemovedFromProfile, void());

  MOCK_METHOD0(Shutdown, void());
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_SERVICE_H_
