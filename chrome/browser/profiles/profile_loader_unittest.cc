// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_loader.h"

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

class TestProfileLoader : public ProfileLoader {
 public:
  TestProfileLoader() : ProfileLoader(NULL) {}
  virtual ~TestProfileLoader() {}

  MOCK_METHOD1(GetProfileByPath, Profile*(const base::FilePath&));
  MOCK_METHOD5(CreateProfileAsync, void(const base::FilePath&,
                                        const ProfileManager::CreateCallback&,
                                        const string16&,
                                        const string16&,
                                        const std::string&));

  void SetCreateCallback(const base::FilePath& path,
                         const ProfileManager::CreateCallback& callback) {
    callbacks_[path] = callback;
  }

  void RunCreateCallback(const base::FilePath& path,
                         Profile* profile,
                         Profile::CreateStatus status) {
    callbacks_[path].Run(profile, status);
  }

 private:
  std::map<base::FilePath, ProfileManager::CreateCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(TestProfileLoader);
};

class MockCallback : public base::RefCountedThreadSafe<MockCallback> {
 public:
  MockCallback();
  MOCK_METHOD1(Run, void(Profile*));

 protected:
  friend class base::RefCountedThreadSafe<MockCallback>;
  virtual ~MockCallback();

 private:
  DISALLOW_COPY_AND_ASSIGN(MockCallback);
};

MockCallback::MockCallback() {}
MockCallback::~MockCallback() {}

TEST(ProfileLoaderTest, LoadProfileInvalidatingOtherLoads) {
  TestingProfile profile;
  base::FilePath fake_profile_path_1 =
      base::FilePath::FromUTF8Unsafe("fake/profile 1");
  base::FilePath fake_profile_path_2 =
      base::FilePath::FromUTF8Unsafe("fake/profile 2");

  TestProfileLoader loader;
  EXPECT_FALSE(loader.IsAnyProfileLoading());

  // path_1 never loads.
  EXPECT_CALL(loader, GetProfileByPath(fake_profile_path_1))
      .WillRepeatedly(Return(static_cast<Profile*>(NULL)));
  EXPECT_CALL(loader,
              CreateProfileAsync(fake_profile_path_1, _, _, _, std::string()))
      .WillRepeatedly(WithArgs<0, 1>(
          Invoke(&loader, &TestProfileLoader::SetCreateCallback)));

  // path_2 loads after the first request.
  EXPECT_CALL(loader, GetProfileByPath(fake_profile_path_2))
      .WillOnce(Return(static_cast<Profile*>(NULL)))
      .WillRepeatedly(Return(&profile));
  EXPECT_CALL(loader,
              CreateProfileAsync(fake_profile_path_2, _, _, _, std::string()))
      .WillRepeatedly(WithArgs<0, 1>(
          Invoke(&loader, &TestProfileLoader::SetCreateCallback)));

  // Try to load both paths twice.
  // path_1_load is never called because it is first invalidated by the load
  // request for (path_2), and then invalidated manually.
  // path_2_load is called both times.
  StrictMock<MockCallback>* path_1_load = new StrictMock<MockCallback>();
  StrictMock<MockCallback>* path_2_load = new StrictMock<MockCallback>();
  EXPECT_CALL(*path_2_load, Run(&profile))
      .Times(2);

  // Try to load path_1.
  loader.LoadProfileInvalidatingOtherLoads(
      fake_profile_path_1, base::Bind(&MockCallback::Run, path_1_load));
  EXPECT_TRUE(loader.IsAnyProfileLoading());

  // Try to load path_2, this invalidates the previous request.
  loader.LoadProfileInvalidatingOtherLoads(
      fake_profile_path_2, base::Bind(&MockCallback::Run, path_2_load));

  // Finish the load request for path_1, then for path_2.
  loader.RunCreateCallback(fake_profile_path_1, &profile,
                           Profile::CREATE_STATUS_INITIALIZED);
  loader.RunCreateCallback(fake_profile_path_2, &profile,
                           Profile::CREATE_STATUS_INITIALIZED);
  EXPECT_FALSE(loader.IsAnyProfileLoading());

  // The second request for path_2 should return immediately.
  loader.LoadProfileInvalidatingOtherLoads(
      fake_profile_path_2, base::Bind(&MockCallback::Run, path_2_load));

  // Make a second request for path_1, and invalidate it.
  loader.LoadProfileInvalidatingOtherLoads(
      fake_profile_path_1, base::Bind(&MockCallback::Run, path_1_load));
  loader.InvalidatePendingProfileLoads();
}

}  // namespace
