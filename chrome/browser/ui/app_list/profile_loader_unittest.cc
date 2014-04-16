// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/profile_loader.h"
#include "chrome/browser/ui/app_list/test/fake_profile.h"
#include "chrome/browser/ui/app_list/test/fake_profile_store.h"
#include "testing/gtest/include/gtest/gtest.h"

class ProfileLoaderUnittest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    last_callback_result_ = NULL;
    profile1_.reset(
        new FakeProfile("p1", base::FilePath(FILE_PATH_LITERAL("profile1"))));
    profile2_.reset(
        new FakeProfile("p2", base::FilePath(FILE_PATH_LITERAL("profile2"))));

    profile_store_.reset(new FakeProfileStore(
        base::FilePath(FILE_PATH_LITERAL("udd"))));
    loader_.reset(new ProfileLoader(profile_store_.get()));
  }

  void StartLoadingProfile(Profile* profile) {
    loader_->LoadProfileInvalidatingOtherLoads(
        profile->GetPath(),
        base::Bind(&ProfileLoaderUnittest::OnProfileLoaded,
                  base::Unretained(this)));
  }

  void FinishLoadingProfile(Profile* profile) {
    profile_store_->LoadProfile(profile);
  }

  void OnProfileLoaded(Profile* profile) {
    last_callback_result_ = profile;
  }

  bool HasKeepAlive() const {
    return loader_->keep_alive_.get() != NULL;
  }

 protected:
  scoped_ptr<ProfileLoader> loader_;
  scoped_ptr<FakeProfileStore> profile_store_;
  scoped_ptr<FakeProfile> profile1_;
  scoped_ptr<FakeProfile> profile2_;
  Profile* last_callback_result_;
};

TEST_F(ProfileLoaderUnittest, LoadASecondProfileBeforeTheFirstFinishes) {
  EXPECT_FALSE(loader_->IsAnyProfileLoading());

  StartLoadingProfile(profile1_.get());
  EXPECT_TRUE(loader_->IsAnyProfileLoading());

  StartLoadingProfile(profile2_.get());
  FinishLoadingProfile(profile1_.get());
  EXPECT_EQ(NULL, last_callback_result_);

  FinishLoadingProfile(profile2_.get());
  EXPECT_EQ(profile2_.get(), last_callback_result_);
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
}

TEST_F(ProfileLoaderUnittest, TestKeepsAliveWhileLoadingProfiles) {
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
  EXPECT_FALSE(HasKeepAlive());

  StartLoadingProfile(profile1_.get());
  EXPECT_TRUE(loader_->IsAnyProfileLoading());
  EXPECT_TRUE(HasKeepAlive());

  FinishLoadingProfile(profile1_.get());
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
  EXPECT_FALSE(HasKeepAlive());
}

TEST_F(ProfileLoaderUnittest, TestKeepsAliveWhileLoadingMultipleProfiles) {
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
  EXPECT_FALSE(HasKeepAlive());

  StartLoadingProfile(profile1_.get());
  EXPECT_TRUE(loader_->IsAnyProfileLoading());
  EXPECT_TRUE(HasKeepAlive());

  StartLoadingProfile(profile2_.get());
  EXPECT_TRUE(loader_->IsAnyProfileLoading());
  EXPECT_TRUE(HasKeepAlive());

  FinishLoadingProfile(profile1_.get());
  EXPECT_TRUE(loader_->IsAnyProfileLoading());
  EXPECT_TRUE(HasKeepAlive());

  FinishLoadingProfile(profile2_.get());
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
  EXPECT_FALSE(HasKeepAlive());
}

TEST_F(ProfileLoaderUnittest, TestInvalidatingCurrentLoad) {
  EXPECT_FALSE(loader_->IsAnyProfileLoading());
  EXPECT_FALSE(HasKeepAlive());

  StartLoadingProfile(profile1_.get());
  loader_->InvalidatePendingProfileLoads();
  // The profile is still considered loading even though we will do nothing when
  // it gets here.
  EXPECT_TRUE(loader_->IsAnyProfileLoading());
  EXPECT_TRUE(HasKeepAlive());

  FinishLoadingProfile(profile1_.get());
  EXPECT_EQ(NULL, last_callback_result_);
}
