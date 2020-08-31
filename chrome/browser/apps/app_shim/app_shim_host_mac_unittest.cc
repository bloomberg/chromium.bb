// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"

#include <unistd.h>

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestingAppShim : public chrome::mojom::AppShim {
 public:
  TestingAppShim() {}

  chrome::mojom::AppShimHostBootstrap::OnShimConnectedCallback
  GetOnShimConnectedCallback() {
    return base::BindOnce(&TestingAppShim::OnShimConnectedDone,
                          base::Unretained(this));
  }
  mojo::PendingReceiver<chrome::mojom::AppShimHostBootstrap>
  GetHostBootstrapReceiver() {
    return host_bootstrap_remote_.BindNewPipeAndPassReceiver();
  }

  chrome::mojom::AppShimLaunchResult GetLaunchResult() const {
    EXPECT_TRUE(received_launch_done_result_);
    return launch_done_result_;
  }

 private:
  void OnShimConnectedDone(
      chrome::mojom::AppShimLaunchResult result,
      mojo::PendingReceiver<chrome::mojom::AppShim> app_shim_receiver) {
    received_launch_done_result_ = true;
    launch_done_result_ = result;
  }

  // chrome::mojom::AppShim implementation.
  void CreateRemoteCocoaApplication(
      mojo::PendingAssociatedReceiver<remote_cocoa::mojom::Application>
          receiver) override {}
  void CreateCommandDispatcherForWidget(uint64_t widget_id) override {}
  void SetUserAttention(
      chrome::mojom::AppShimAttentionType attention_type) override {}
  void SetBadgeLabel(const std::string& badge_label) override {}
  void UpdateProfileMenu(std::vector<chrome::mojom::ProfileMenuItemPtr>
                             profile_menu_items) override {}

  bool received_launch_done_result_ = false;
  chrome::mojom::AppShimLaunchResult launch_done_result_ =
      chrome::mojom::AppShimLaunchResult::kSuccess;

  mojo::Remote<chrome::mojom::AppShimHostBootstrap> host_bootstrap_remote_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShim);
};

class TestingAppShimHost : public AppShimHost {
 public:
  TestingAppShimHost(Client* client,
                     const std::string& app_id,
                     const base::FilePath& profile_path)
      : AppShimHost(client,
                    app_id,
                    profile_path,
                    false /* uses_remote_views */) {}
  ~TestingAppShimHost() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHost);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  explicit TestingAppShimHostBootstrap(
      mojo::PendingReceiver<chrome::mojom::AppShimHostBootstrap> host_receiver)
      : AppShimHostBootstrap(getpid()), test_weak_factory_(this) {
    // AppShimHost will bind to the receiver from ServeChannel. For testing
    // purposes, have this receiver passed in at creation.
    host_bootstrap_receiver_.Bind(std::move(host_receiver));
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return test_weak_factory_.GetWeakPtr();
  }

  using AppShimHostBootstrap::OnShimConnected;

 private:
  base::WeakPtrFactory<TestingAppShimHostBootstrap> test_weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestProfileDir[] = "Profile 1";

// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL TestUrl() {
  return GURL("https://example.com");
}

class AppShimHostTest : public testing::Test,
                        public AppShimHostBootstrap::Client,
                        public AppShimHost::Client {
 public:
  AppShimHostTest() { task_runner_ = base::ThreadTaskRunnerHandle::Get(); }
  ~AppShimHostTest() override {}

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }
  AppShimHost* host() { return host_.get(); }
  chrome::mojom::AppShimHost* GetMojoHost() { return host_remote_.get(); }

  void DoOnShimConnected(chrome::mojom::AppShimLaunchType launch_type) {
    auto app_shim_info = chrome::mojom::AppShimInfo::New();
    app_shim_info->profile_path = base::FilePath(kTestProfileDir);
    app_shim_info->app_id = kTestAppId;
    app_shim_info->app_url = TestUrl();
    app_shim_info->launch_type = launch_type;
    // Ownership of TestingAppShimHostBootstrap will be transferred to its host.
    (new TestingAppShimHostBootstrap(shim_->GetHostBootstrapReceiver()))
        ->OnShimConnected(host_remote_.BindNewPipeAndPassReceiver(),
                          std::move(app_shim_info),
                          shim_->GetOnShimConnectedCallback());
  }

  chrome::mojom::AppShimLaunchResult GetLaunchResult() {
    RunUntilIdle();
    return shim_->GetLaunchResult();
  }

  void SimulateDisconnect() { host_remote_.reset(); }

 protected:
  // AppShimHostBootstrap::Client:
  void OnShimProcessConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override {
    ++launch_count_;
    if (bootstrap->GetLaunchType() == chrome::mojom::AppShimLaunchType::kNormal)
      ++launch_now_count_;
    host_ = std::make_unique<TestingAppShimHost>(this, bootstrap->GetAppId(),
                                                 bootstrap->GetProfilePath());
    if (launch_result_ == chrome::mojom::AppShimLaunchResult::kSuccess)
      host_->OnBootstrapConnected(std::move(bootstrap));
    else
      bootstrap->OnFailedToConnectToHost(launch_result_);
  }

  // AppShimHost::Client:
  void OnShimLaunchRequested(
      AppShimHost* host,
      bool recreate_shims,
      apps::ShimLaunchedCallback launched_callback,
      apps::ShimTerminatedCallback terminated_callback) override {}
  void OnShimProcessDisconnected(AppShimHost* host) override {
    DCHECK_EQ(host, host_.get());
    host_ = nullptr;
    ++close_count_;
  }
  void OnShimFocus(AppShimHost* host) override { ++focus_count_; }
  void OnShimOpenedFiles(AppShimHost* host,
                         const std::vector<base::FilePath>& files) override {}
  void OnShimSelectedProfile(AppShimHost* host,
                             const base::FilePath& profile_path) override {}

  chrome::mojom::AppShimLaunchResult launch_result_ =
      chrome::mojom::AppShimLaunchResult::kSuccess;
  int launch_count_ = 0;
  int launch_now_count_ = 0;
  int close_count_ = 0;
  int focus_count_ = 0;

 private:
  void SetUp() override {
    testing::Test::SetUp();
    shim_.reset(new TestingAppShim());
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestingAppShim> shim_;

  std::unique_ptr<AppShimHostBootstrap> launched_bootstrap_;

  // AppShimHost will destroy itself in AppShimHost::Close, so use a weak
  // pointer here to avoid lifetime issues.
  std::unique_ptr<TestingAppShimHost> host_;
  mojo::Remote<chrome::mojom::AppShimHost> host_remote_;

  DISALLOW_COPY_AND_ASSIGN(AppShimHostTest);
};

}  // namespace

TEST_F(AppShimHostTest, TestOnShimConnectedWithHandler) {
  AppShimHostBootstrap::SetClient(this);
  DoOnShimConnected(chrome::mojom::AppShimLaunchType::kNormal);
  EXPECT_EQ(kTestAppId, host()->GetAppId());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(1, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);

  GetMojoHost()->FocusApp();
  RunUntilIdle();
  EXPECT_EQ(1, focus_count_);

  SimulateDisconnect();
  RunUntilIdle();
  EXPECT_EQ(1, close_count_);
  EXPECT_EQ(nullptr, host());
  AppShimHostBootstrap::SetClient(nullptr);
}

TEST_F(AppShimHostTest, TestNoLaunchNow) {
  AppShimHostBootstrap::SetClient(this);
  DoOnShimConnected(chrome::mojom::AppShimLaunchType::kRegisterOnly);
  EXPECT_EQ(kTestAppId, host()->GetAppId());
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kSuccess, GetLaunchResult());
  EXPECT_EQ(1, launch_count_);
  EXPECT_EQ(0, launch_now_count_);
  EXPECT_EQ(0, focus_count_);
  EXPECT_EQ(0, close_count_);
  AppShimHostBootstrap::SetClient(nullptr);
}

TEST_F(AppShimHostTest, TestFailLaunch) {
  AppShimHostBootstrap::SetClient(this);
  launch_result_ = chrome::mojom::AppShimLaunchResult::kAppNotFound;
  DoOnShimConnected(chrome::mojom::AppShimLaunchType::kNormal);
  EXPECT_EQ(chrome::mojom::AppShimLaunchResult::kAppNotFound,
            GetLaunchResult());
  AppShimHostBootstrap::SetClient(nullptr);
}
