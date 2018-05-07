// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_host.h"

#include <utility>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "chromeos/disks/mock_disk_mount_manager.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drivefs {
namespace {

using testing::_;

class TestingMojoConnectionDelegate
    : public DriveFsHost::MojoConnectionDelegate {
 public:
  TestingMojoConnectionDelegate(
      mojom::DriveFsBootstrapPtrInfo pending_bootstrap)
      : pending_bootstrap_(std::move(pending_bootstrap)) {}

  mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() override {
    return std::move(pending_bootstrap_);
  }

  void AcceptMojoConnection(base::ScopedFD handle) override {}

 private:
  mojom::DriveFsBootstrapPtrInfo pending_bootstrap_;

  DISALLOW_COPY_AND_ASSIGN(TestingMojoConnectionDelegate);
};

class DriveFsHostTest : public ::testing::Test,
                        public mojom::DriveFsBootstrap,
                        public mojom::DriveFs {
 public:
  DriveFsHostTest() : bootstrap_binding_(this), binding_(this) {}

 protected:
  void SetUp() override {
    testing::Test::SetUp();
    profile_path_ = base::FilePath(FILE_PATH_LITERAL("/path/to/profile"));
    account_id_ = AccountId::FromUserEmailGaiaId("test@example.com", "ID");

    disk_manager_ = new chromeos::disks::MockDiskMountManager;
    // Takes ownership of |disk_manager_|.
    chromeos::disks::DiskMountManager::InitializeForTesting(disk_manager_);

    host_ = std::make_unique<DriveFsHost>(
        profile_path_, account_id_,
        base::BindRepeating(&DriveFsHostTest::CreateMojoConnectionDelegate,
                            base::Unretained(this)));
  }

  void TearDown() override {
    host_.reset();
    disk_manager_ = nullptr;
    chromeos::disks::DiskMountManager::Shutdown();
  }

  std::unique_ptr<DriveFsHost::MojoConnectionDelegate>
  CreateMojoConnectionDelegate() {
    DCHECK(pending_bootstrap_);
    return std::make_unique<TestingMojoConnectionDelegate>(
        std::move(pending_bootstrap_));
  }

  void DispatchMountEvent(
      chromeos::disks::DiskMountManager::MountEvent event,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info) {
    static_cast<chromeos::disks::DiskMountManager::Observer&>(*host_)
        .OnMountEvent(event, error_code, mount_info);
  }

  std::string StartMount() {
    std::string source;
    EXPECT_CALL(
        *disk_manager_,
        MountPath(
            testing::AllOf(testing::StartsWith("drivefs://"),
                           testing::EndsWith("@/path/to/profile/GCache/v2/ID")),
            "", "", _, chromeos::MOUNT_ACCESS_MODE_READ_WRITE))
        .WillOnce(testing::SaveArg<0>(&source));

    mojom::DriveFsBootstrapPtrInfo bootstrap;
    bootstrap_binding_.Bind(mojo::MakeRequest(&bootstrap));
    pending_bootstrap_ = std::move(bootstrap);

    EXPECT_TRUE(host_->Mount());
    testing::Mock::VerifyAndClear(&disk_manager_);

    return base::SplitStringPiece(
               base::StringPiece(source).substr(strlen("drivefs://")), "@",
               base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)[0]
        .as_string();
  }

  void DispatchMountSuccessEvent(const std::string& token) {
    DispatchMountEvent(
        chromeos::disks::DiskMountManager::MOUNTING, chromeos::MOUNT_ERROR_NONE,
        {base::StrCat({"drivefs://", token, "@/path/to/profile/GCache/v2/ID"}),
         "/media/drivefsroot/ID",
         chromeos::MOUNT_TYPE_ARCHIVE,
         {}});
  }

  void DoMount() {
    auto token = StartMount();
    DispatchMountSuccessEvent(token);

    base::RunLoop run_loop;
    bootstrap_binding_.set_connection_error_handler(run_loop.QuitClosure());
    ASSERT_TRUE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
    run_loop.Run();
  }

  void Init(mojom::DriveFsConfigurationPtr config,
            mojom::DriveFsRequest drive_fs,
            mojom::DriveFsDelegatePtr delegate) override {
    EXPECT_EQ("test@example.com", config->user_email);
    binding_.Bind(std::move(drive_fs));
    delegate_ptr_ = std::move(delegate);
  }

  base::FilePath profile_path_;
  base::test::ScopedTaskEnvironment task_environment_;
  AccountId account_id_;
  chromeos::disks::MockDiskMountManager* disk_manager_;
  std::unique_ptr<DriveFsHost> host_;

  mojo::Binding<mojom::DriveFsBootstrap> bootstrap_binding_;
  mojo::Binding<mojom::DriveFs> binding_;
  mojom::DriveFsDelegatePtr delegate_ptr_;

  mojom::DriveFsBootstrapPtrInfo pending_bootstrap_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsHostTest);
};

TEST_F(DriveFsHostTest, Basic) {
  ASSERT_NO_FATAL_FAILURE(DoMount());
}

TEST_F(DriveFsHostTest, UnmountAfterMountComplete) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));
  base::RunLoop run_loop;
  binding_.set_connection_error_handler(run_loop.QuitClosure());
  host_->Unmount();
  run_loop.Run();
}

TEST_F(DriveFsHostTest, UnmountBeforeMountEvent) {
  auto token = StartMount();
  host_->Unmount();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, UnmountBeforeMojoConnection) {
  auto token = StartMount();
  DispatchMountSuccessEvent(token);
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));

  host_->Unmount();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, DestroyBeforeMountEvent) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);

  host_.reset();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, DestroyBeforeMojoConnection) {
  auto token = StartMount();
  DispatchMountSuccessEvent(token);
  EXPECT_CALL(*disk_manager_, UnmountPath("/media/drivefsroot/ID",
                                          chromeos::UNMOUNT_OPTIONS_NONE, _));

  host_.reset();
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, ObserveOtherMount) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);

  DispatchMountEvent(chromeos::disks::DiskMountManager::MOUNTING,
                     chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED,
                     {"some/other/mount/event",
                      "/some/other/mount/point",
                      chromeos::MOUNT_TYPE_DEVICE,
                      {}});
  DispatchMountEvent(
      chromeos::disks::DiskMountManager::UNMOUNTING, chromeos::MOUNT_ERROR_NONE,
      {base::StrCat({"drivefs://", token, "@/path/to/profile/GCache/v2/ID"}),
       "/media/drivefsroot/ID",
       chromeos::MOUNT_TYPE_ARCHIVE,
       {}});
  host_->Unmount();
}

TEST_F(DriveFsHostTest, MountError) {
  auto token = StartMount();
  EXPECT_CALL(*disk_manager_, UnmountPath(_, _, _)).Times(0);

  DispatchMountEvent(
      chromeos::disks::DiskMountManager::MOUNTING,
      chromeos::MOUNT_ERROR_DIRECTORY_CREATION_FAILED,
      {base::StrCat({"drivefs://", token, "@/path/to/profile/GCache/v2/ID"}),
       "/media/drivefsroot/ID",
       chromeos::MOUNT_TYPE_ARCHIVE,
       {}});
  EXPECT_FALSE(PendingConnectionManager::Get().OpenIpcChannel(token, {}));
}

TEST_F(DriveFsHostTest, MountWhileAlreadyMounted) {
  DoMount();
  EXPECT_FALSE(host_->Mount());
}

TEST_F(DriveFsHostTest, NonGaiaAccount) {
  EXPECT_CALL(*disk_manager_, MountPath(_, _, _, _, _)).Times(0);
  AccountId non_gaia_account = AccountId::FromUserEmail("test2@example.com");
  host_ = std::make_unique<DriveFsHost>(profile_path_, non_gaia_account);
  EXPECT_FALSE(host_->Mount());
}

TEST_F(DriveFsHostTest, GetAccessToken) {
  ASSERT_NO_FATAL_FAILURE(DoMount());

  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  delegate_ptr_->GetAccessToken(
      "client ID", "app ID", {"scope1", "scope2"},
      base::BindLambdaForTesting(
          [&](mojom::AccessTokenStatus status, const std::string& token) {
            EXPECT_EQ(mojom::AccessTokenStatus::kAuthError, status);
            EXPECT_TRUE(token.empty());
            std::move(quit_closure).Run();
          }));
  run_loop.Run();
}

}  // namespace
}  // namespace drivefs
