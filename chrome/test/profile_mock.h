// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PROFILE_MOCK_H__
#define CHROME_TEST_PROFILE_MOCK_H__
#pragma once

#include "chrome/test/testing_profile.h"

#include "testing/gmock/include/gmock/gmock.h"

class ProfileMock : public TestingProfile {
 public:
  ProfileMock();
  virtual ~ProfileMock();

  MOCK_METHOD0(GetBookmarkModel, BookmarkModel*());
  MOCK_METHOD1(GetHistoryService, HistoryService*(ServiceAccessType access));
  MOCK_METHOD0(GetHistoryServiceWithoutCreating, HistoryService*());
  MOCK_METHOD1(GetWebDataService, WebDataService*(ServiceAccessType access));
  MOCK_METHOD0(GetPersonalDataManager, PersonalDataManager*());
  MOCK_METHOD1(GetPasswordStore, PasswordStore* (ServiceAccessType access));
  MOCK_METHOD0(GetTokenService, TokenService*());
  MOCK_METHOD0(GetProfileSyncService, ProfileSyncService*());
};

#endif  // CHROME_TEST_PROFILE_MOCK_H__
