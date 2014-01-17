// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gcm/engine/user_list.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "google_apis/gcm/engine/gcm_store_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

class GCMClientDelegate : public GCMClient::Delegate {
 public:
  explicit GCMClientDelegate(const std::string& username);
  virtual ~GCMClientDelegate();

  const std::string& GetUsername() const { return username_; }

  // Overrides of GCMClientDelegate:
  virtual void OnCheckInFinished(const GCMClient::CheckinInfo& checkin_info,
                                 GCMClient::Result result) OVERRIDE {}
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE {}
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE {}
  virtual void OnMessageReceived(const std::string& app_id,
                                 const GCMClient::IncomingMessage& message)
      OVERRIDE {}
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE {}
  virtual void OnMessageSendError(const std::string& app_id,
                                  const std::string& message_id,
                                  GCMClient::Result result) OVERRIDE {}
  virtual GCMClient::CheckinInfo GetCheckinInfo() const OVERRIDE {
    return GCMClient::CheckinInfo();
  }
  virtual void OnLoadingCompleted() OVERRIDE {}

 private:
  std::string username_;
};

}  // namespace

class UserListTest : public testing::Test {
 public:
  UserListTest();
  virtual ~UserListTest();

  virtual void SetUp() OVERRIDE;

  static size_t GetListSize(const UserList* user_list);
  void SetDelegateCallback(const std::string& username,
                           int64 user_serial_number);

  scoped_ptr<UserList> BuildUserList();

  void PumpLoop();

  void UpdateCallback(bool success);

  void Initialize(UserList* user_list, const GCMStore::LoadResult& result);

  void ResetLoop();

 protected:
  int64 last_assigned_serial_number_;
  std::string last_assigned_username_;
  scoped_ptr<GCMStore> gcm_store_;

 private:
  base::ScopedTempDir temp_directory_;
  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;
};

UserListTest::UserListTest()
    : last_assigned_serial_number_(gcm::kInvalidSerialNumber) {}

UserListTest::~UserListTest() {}

void UserListTest::SetUp() {
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
}

// static
size_t UserListTest::GetListSize(const UserList* user_list) {
  return user_list->delegates_.size();
}

void UserListTest::SetDelegateCallback(const std::string& username,
                                       int64 user_serial_number) {
  last_assigned_username_ = username;
  last_assigned_serial_number_ = user_serial_number;
  ResetLoop();
}

scoped_ptr<UserList> UserListTest::BuildUserList() {
  gcm_store_.reset(new GCMStoreImpl(temp_directory_.path(),
                                    message_loop_.message_loop_proxy()));
  return scoped_ptr<UserList>(new UserList(gcm_store_.get()));
}

void UserListTest::Initialize(UserList* user_list,
                              const GCMStore::LoadResult& result) {
  ASSERT_TRUE(result.success);
  user_list->Initialize(result.serial_number_mappings);
  ResetLoop();
}

void UserListTest::ResetLoop() {
  if (run_loop_ && run_loop_->running())
    run_loop_->Quit();
}

void UserListTest::PumpLoop() {
  run_loop_.reset(new base::RunLoop);
  run_loop_->Run();
}

void UserListTest::UpdateCallback(bool success) { ASSERT_TRUE(success); }

GCMClientDelegate::GCMClientDelegate(const std::string& username)
    : username_(username) {}

GCMClientDelegate::~GCMClientDelegate() {}

// Make sure it is possible to add a delegate, and that it is assigned a serial
// number.
TEST_F(UserListTest, SetDelegateAndCheckSerialNumberAssignment) {
  scoped_ptr<UserList> user_list(BuildUserList());
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // First adding a delegate.
  scoped_ptr<GCMClientDelegate> delegate(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  // Verify the record was created.
  EXPECT_EQ(1u, GetListSize(user_list.get()));
  // Verify username and serial number were assigned.
  EXPECT_EQ(delegate->GetUsername(), last_assigned_username_);
  EXPECT_NE(gcm::kInvalidSerialNumber, last_assigned_serial_number_);
  // Check that a serial number was assigned to delegate.
  EXPECT_EQ(last_assigned_serial_number_,
            user_list->GetSerialNumberForUsername(delegate->GetUsername()));
}

// Get the delegate that was added to the list by both serial number and
// username.
TEST_F(UserListTest, GetDelegate) {
  scoped_ptr<UserList> user_list(BuildUserList());
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // Start by adding a delegate and a serial number.
  scoped_ptr<GCMClientDelegate> delegate(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(delegate.get(),
            user_list->GetDelegateBySerialNumber(last_assigned_serial_number_));
  EXPECT_EQ(delegate.get(),
            user_list->GetDelegateByUsername(last_assigned_username_));
}

// Make sure that correct mapping between username of the delegate and a serial
// number is preserved when loading the user list. Also verify that setting a
// delegate after load works correctly. (Finds the existing mapping entry.)
TEST_F(UserListTest, LoadUserEntrySetDelegate) {
  scoped_ptr<UserList> user_list(BuildUserList());
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // Start by adding a delegate and a serial number.
  scoped_ptr<GCMClientDelegate> delegate(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  // Reload the GCM User List.
  user_list = BuildUserList().Pass();
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // Verify a single record was loaded, with matching username, but no delegate.
  EXPECT_EQ(1u, GetListSize(user_list.get()));
  int64 serial_number =
      user_list->GetSerialNumberForUsername(delegate->GetUsername());
  EXPECT_EQ(last_assigned_serial_number_, serial_number);
  EXPECT_EQ(NULL, user_list->GetDelegateBySerialNumber(serial_number));
  EXPECT_EQ(NULL, user_list->GetDelegateByUsername(delegate->GetUsername()));

  // After loading is complete, Delegates will start adding itself looking for
  // their serial numbers. Check that correct matches are found and new records
  // not created.
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  EXPECT_EQ(1u, GetListSize(user_list.get()));
  serial_number =
      user_list->GetSerialNumberForUsername(delegate->GetUsername());
  EXPECT_EQ(last_assigned_serial_number_, serial_number);
  EXPECT_EQ(delegate.get(),
            user_list->GetDelegateBySerialNumber(serial_number));
  EXPECT_EQ(delegate.get(),
            user_list->GetDelegateByUsername(delegate->GetUsername()));
}

// Check that it is possible to add multiple delegates to the user list.
TEST_F(UserListTest, AddMultipleDelegates) {
  scoped_ptr<UserList> user_list(BuildUserList());
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // Start by adding a delegate and a serial number.
  scoped_ptr<GCMClientDelegate> delegate1(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate1->GetUsername(),
      delegate1.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  int64 serial_number1 = last_assigned_serial_number_;

  scoped_ptr<GCMClientDelegate> delegate2(new GCMClientDelegate("test_user_2"));
  user_list->SetDelegate(
      delegate2->GetUsername(),
      delegate2.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  int64 serial_number2 = last_assigned_serial_number_;

  // Ensuring that serial numbers are different.
  EXPECT_EQ(2u, GetListSize(user_list.get()));

  // Reading the user entries.
  user_list = BuildUserList().Pass();
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // Serial numbers stay the same, but there are no delegates assigned.
  EXPECT_EQ(2u, GetListSize(user_list.get()));
  EXPECT_EQ(NULL, user_list->GetDelegateByUsername(delegate1->GetUsername()));
  EXPECT_EQ(NULL, user_list->GetDelegateBySerialNumber(serial_number1));
  EXPECT_EQ(NULL, user_list->GetDelegateByUsername(delegate2->GetUsername()));
  EXPECT_EQ(NULL, user_list->GetDelegateBySerialNumber(serial_number2));

  // Setting and checking a delegate on the second user.
  user_list->SetDelegate(
      delegate2->GetUsername(),
      delegate2.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  // First user still does not have a delegate.
  EXPECT_EQ(NULL, user_list->GetDelegateByUsername(delegate1->GetUsername()));
  EXPECT_EQ(NULL, user_list->GetDelegateBySerialNumber(serial_number1));
  // Second user has a delegate set.
  EXPECT_EQ(delegate2.get(),
            user_list->GetDelegateByUsername(delegate2->GetUsername()));
  EXPECT_EQ(delegate2.get(),
            user_list->GetDelegateBySerialNumber(serial_number2));
}

// Adding a delegate before the user list is initialized. Verifies that serial
// number assignment is postponed until after initialization.
TEST_F(UserListTest, AddDelegateThenInitializeWithoutSerialNumber) {
  scoped_ptr<UserList> user_list(BuildUserList());

  // Add a delegate first.
  scoped_ptr<GCMClientDelegate> delegate(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));

  EXPECT_EQ(gcm::kInvalidSerialNumber, last_assigned_serial_number_);
  EXPECT_EQ("", last_assigned_username_);

  // Now run the initialization.
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  // Need to pump twice, due to initialization triggering additional set of
  // callbacks to be run.
  PumpLoop();
  PumpLoop();

  EXPECT_EQ(last_assigned_serial_number_,
            user_list->GetSerialNumberForUsername(delegate->GetUsername()));
  EXPECT_EQ(delegate->GetUsername(), last_assigned_username_);
}

// Adding a delegate that already has a serial number on a subsequent restart of
// the user list and prior to initialization. Expects to assing correct serial
// number based on existing mappings.
TEST_F(UserListTest, AddDelegateThenInitializeWithSerialNumber) {
  scoped_ptr<UserList> user_list(BuildUserList());
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  PumpLoop();

  // First add a delegate to the list so that serial number is persisted.
  scoped_ptr<GCMClientDelegate> delegate(new GCMClientDelegate("test_user_1"));
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));
  PumpLoop();

  last_assigned_serial_number_ = gcm::kInvalidSerialNumber;
  last_assigned_username_ = "";

  // Resetting the user list to make sure it is not initialized.
  user_list = BuildUserList().Pass();

  // Add a delegate again, this time no callback is expected until the list is
  // initialized.
  user_list->SetDelegate(
      delegate->GetUsername(),
      delegate.get(),
      base::Bind(&UserListTest::SetDelegateCallback, base::Unretained(this)));

  // Now run the initialization.
  gcm_store_->Load(base::Bind(
      &UserListTest::Initialize, base::Unretained(this), user_list.get()));
  // Need to pump twice, due to initialization triggering additional set of
  // callbacks to be run.
  PumpLoop();
  PumpLoop();

  EXPECT_EQ(last_assigned_serial_number_,
            user_list->GetSerialNumberForUsername(delegate->GetUsername()));
  EXPECT_EQ(delegate->GetUsername(), last_assigned_username_);
}

}  // namespace gcm
