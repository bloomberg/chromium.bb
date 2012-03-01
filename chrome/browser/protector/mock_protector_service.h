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

  MOCK_METHOD1(DismissChange, void(BaseSettingChange* change));
  MOCK_METHOD2(ApplyChange, void(BaseSettingChange* change, Browser*));
  MOCK_METHOD2(DiscardChange, void(BaseSettingChange* change, Browser*));

  MOCK_METHOD2(OpenTab, void(const GURL&, Browser*));

  MOCK_METHOD2(OnApplyChange, void(SettingsChangeGlobalError* error, Browser*));
  MOCK_METHOD2(OnDiscardChange, void(SettingsChangeGlobalError* error,
                                     Browser*));
  MOCK_METHOD1(OnDecisionTimeout, void(SettingsChangeGlobalError* error));
  MOCK_METHOD1(OnRemovedFromProfile, void(SettingsChangeGlobalError* error));

  MOCK_METHOD0(Shutdown, void());
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_MOCK_PROTECTOR_SERVICE_H_
