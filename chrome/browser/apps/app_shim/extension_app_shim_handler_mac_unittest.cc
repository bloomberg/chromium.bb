// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/apps/app_shim/app_shim_host_bootstrap_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using extensions::Extension;
typedef extensions::AppWindowRegistry::AppWindowList AppWindowList;

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

class MockDelegate : public ExtensionAppShimHandler::Delegate {
 public:
  virtual ~MockDelegate() {}

  base::FilePath GetFullProfilePath(const base::FilePath& relative_path) {
    return relative_path;
  }
  MOCK_METHOD1(ProfileExistsForPath, bool(const base::FilePath&));
  MOCK_METHOD1(ProfileForPath, Profile*(const base::FilePath&));
  void LoadProfileAsync(const base::FilePath& path,
                        base::OnceCallback<void(Profile*)> callback) override {
    CaptureLoadProfileCallback(path, std::move(callback));
  }
  MOCK_METHOD1(IsProfileLockedForPath, bool(const base::FilePath&));

  MOCK_METHOD2(GetWindows, AppWindowList(Profile*, const std::string&));

  MOCK_METHOD2(MaybeGetAppExtension,
               const Extension*(content::BrowserContext*, const std::string&));
  MOCK_METHOD3(EnableExtension, void(Profile*,
                                     const std::string&,
                                     const base::Callback<void()>&));
  MOCK_METHOD3(LaunchApp,
               void(Profile*,
                    const Extension*,
                    const std::vector<base::FilePath>&));
  MOCK_METHOD2(LaunchShim, void(Profile*, const Extension*));
  MOCK_METHOD0(LaunchUserManager, void());

  MOCK_METHOD0(MaybeTerminate, void());

  void SetHostForCreate(AppShimHost* host_for_create) {
    host_for_create_ = host_for_create;
  }
  AppShimHost* CreateHost(const std::string& app_id,
                          const base::FilePath& profile_path) override {
    DCHECK(host_for_create_);
    auto* result = host_for_create_;
    host_for_create_ = nullptr;
    return result;
  }

  void CaptureLoadProfileCallback(const base::FilePath& path,
                                  base::OnceCallback<void(Profile*)> callback) {
    callbacks_[path] = std::move(callback);
  }

  bool RunLoadProfileCallback(
      const base::FilePath& path,
      Profile* profile) {
    std::move(callbacks_[path]).Run(profile);
    return callbacks_.erase(path);
  }

  void RunCallback(const base::Callback<void()>& callback) {
    callback.Run();
  }

 private:
  std::map<base::FilePath, base::OnceCallback<void(Profile*)>> callbacks_;
  AppShimHost* host_for_create_ = nullptr;
};

class TestingExtensionAppShimHandler : public ExtensionAppShimHandler {
 public:
  TestingExtensionAppShimHandler(Delegate* delegate) {
    set_delegate(delegate);
  }
  virtual ~TestingExtensionAppShimHandler() {}

  MOCK_METHOD3(OnShimFocus,
               void(AppShimHost* host,
                    AppShimFocusType,
                    const std::vector<base::FilePath>& files));

  void RealOnShimFocus(AppShimHost* host,
                       AppShimFocusType focus_type,
                       const std::vector<base::FilePath>& files) {
    ExtensionAppShimHandler::OnShimFocus(host, focus_type, files);
  }

  AppShimHost* FindHost(Profile* profile, const std::string& app_id) {
    HostMap::const_iterator it = hosts().find(make_pair(profile, app_id));
    return it == hosts().end() ? NULL : it->second;
  }

  content::NotificationRegistrar& GetRegistrar() { return registrar(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingExtensionAppShimHandler);
};

class TestingAppShimHostBootstrap : public AppShimHostBootstrap {
 public:
  TestingAppShimHostBootstrap(
      const base::FilePath& profile_path,
      const std::string& app_id,
      base::Optional<apps::AppShimLaunchResult>* launch_result,
      apps::AppShimHandler* handler)
      : profile_path_(profile_path),
        app_id_(app_id),
        launch_result_(launch_result),
        handler_(handler),
        weak_factory_(this) {}
  apps::AppShimHandler* GetHandler() override { return handler_; }

  void DoTestLaunch(apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    chrome::mojom::AppShimHostPtr host_ptr;
    LaunchApp(mojo::MakeRequest(&host_ptr), profile_path_, app_id_, launch_type,
              files,
              base::BindOnce(&TestingAppShimHostBootstrap::DoTestLaunchDone,
                             launch_result_));
  }

  static void DoTestLaunchDone(
      base::Optional<apps::AppShimLaunchResult>* launch_result,
      apps::AppShimLaunchResult result,
      chrome::mojom::AppShimRequest app_shim_request) {
    if (launch_result)
      launch_result->emplace(result);
  }

  base::WeakPtr<TestingAppShimHostBootstrap> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::FilePath profile_path_;
  std::string app_id_;
  // Note that |launch_result_| is optional so that we can track whether or not
  // the callback to set it has arrived.
  base::Optional<apps::AppShimLaunchResult>* launch_result_;
  apps::AppShimHandler* const handler_;
  base::WeakPtrFactory<TestingAppShimHostBootstrap> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestingAppShimHostBootstrap);
};

const char kTestAppIdA[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestAppIdB[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

class TestHost : public AppShimHost {
 public:
  TestHost(const base::FilePath& profile_path,
           const std::string& app_id,
           TestingExtensionAppShimHandler* handler)
      : AppShimHost(app_id, profile_path),
        handler_(handler),
        weak_factory_(this) {}

  // Override the GetAppShimHandler for testing.
  apps::AppShimHandler* GetAppShimHandler() const override { return handler_; }

  // Save the result of OnAppLaunchComplete for testing.
  void OnAppLaunchComplete(apps::AppShimLaunchResult result) override {
    app_launch_result.emplace(result);
    AppShimHost::OnAppLaunchComplete(result);
  }
  base::Optional<apps::AppShimLaunchResult> app_launch_result;

  apps::AppShimLaunchResult GetAppLaunchResult() {
    DCHECK(app_launch_result);
    return *app_launch_result;
  }

  base::WeakPtr<TestHost> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  ~TestHost() override {}
  TestingExtensionAppShimHandler* handler_;

  base::WeakPtrFactory<TestHost> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(TestHost);
};

class ExtensionAppShimHandlerTest : public testing::Test {
 protected:
  ExtensionAppShimHandlerTest()
      : delegate_(new MockDelegate),
        handler_(new TestingExtensionAppShimHandler(delegate_)),
        profile_path_a_("Profile A"),
        profile_path_b_("Profile B") {
    bootstrap_aa_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         &bootstrap_aa_result_, handler_.get()))
            ->GetWeakPtr();
    bootstrap_ab_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdB,
                                         &bootstrap_ab_result_, handler_.get()))
            ->GetWeakPtr();
    bootstrap_bb_ =
        (new TestingAppShimHostBootstrap(profile_path_b_, kTestAppIdB,
                                         &bootstrap_bb_result_, handler_.get()))
            ->GetWeakPtr();
    bootstrap_aa_duplicate_ =
        (new TestingAppShimHostBootstrap(profile_path_a_, kTestAppIdA,
                                         &bootstrap_aa_duplicate_result_,
                                         handler_.get()))
            ->GetWeakPtr();

    host_aa_ = (new TestHost(profile_path_a_, kTestAppIdA, handler_.get()))
                   ->GetWeakPtr();
    host_ab_ = (new TestHost(profile_path_a_, kTestAppIdB, handler_.get()))
                   ->GetWeakPtr();
    host_bb_ = (new TestHost(profile_path_b_, kTestAppIdB, handler_.get()))
                   ->GetWeakPtr();
    host_aa_duplicate_ =
        (new TestHost(profile_path_a_, kTestAppIdA, handler_.get()))
            ->GetWeakPtr();

    base::FilePath extension_path("/fake/path");
    extension_a_ = extensions::ExtensionBuilder("Fake Name")
                       .SetLocation(extensions::Manifest::INTERNAL)
                       .SetPath(extension_path)
                       .SetID(kTestAppIdA)
                       .Build();
    extension_b_ = extensions::ExtensionBuilder("Fake Name")
                       .SetLocation(extensions::Manifest::INTERNAL)
                       .SetPath(extension_path)
                       .SetID(kTestAppIdB)
                       .Build();

    EXPECT_CALL(*delegate_, ProfileExistsForPath(profile_path_a_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_a_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
        .WillRepeatedly(Return(&profile_a_));
    EXPECT_CALL(*delegate_, ProfileExistsForPath(profile_path_b_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_b_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*delegate_, ProfileForPath(profile_path_b_))
        .WillRepeatedly(Return(&profile_b_));

    // In most tests, we don't care about the result of GetWindows, it just
    // needs to be non-empty.
    AppWindowList app_window_list;
    app_window_list.push_back(static_cast<extensions::AppWindow*>(NULL));
    EXPECT_CALL(*delegate_, GetWindows(_, _))
        .WillRepeatedly(Return(app_window_list));

    EXPECT_CALL(*delegate_, MaybeGetAppExtension(_, kTestAppIdA))
        .WillRepeatedly(Return(extension_a_.get()));
    EXPECT_CALL(*delegate_, MaybeGetAppExtension(_, kTestAppIdB))
        .WillRepeatedly(Return(extension_b_.get()));
    EXPECT_CALL(*delegate_, LaunchApp(_, _, _))
        .WillRepeatedly(Return());
  }

  ~ExtensionAppShimHandlerTest() override {
    if (host_aa_)
      host_aa_->OnAppClosed();
    if (host_ab_)
      host_ab_->OnAppClosed();
    if (host_bb_)
      host_bb_->OnAppClosed();
    if (host_aa_duplicate_)
      host_aa_duplicate_->OnAppClosed();

    delete bootstrap_aa_.get();
    delete bootstrap_ab_.get();
    delete bootstrap_bb_.get();
    delete bootstrap_aa_duplicate_.get();
  }

  void DoShimLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    base::WeakPtr<TestHost> host,
                    apps::AppShimLaunchType launch_type,
                    const std::vector<base::FilePath>& files) {
    if (host)
      delegate_->SetHostForCreate(host.get());
    bootstrap->DoTestLaunch(launch_type, files);
  }

  void NormalLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                    base::WeakPtr<TestHost> host) {
    DoShimLaunch(bootstrap, host, APP_SHIM_LAUNCH_NORMAL,
                 std::vector<base::FilePath>());
  }

  void RegisterOnlyLaunch(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                          base::WeakPtr<TestHost> host) {
    DoShimLaunch(bootstrap, host, APP_SHIM_LAUNCH_REGISTER_ONLY,
                 std::vector<base::FilePath>());
  }

  // Completely launch a shim host and leave it running.
  void LaunchAndActivate(base::WeakPtr<TestingAppShimHostBootstrap> bootstrap,
                         base::WeakPtr<TestHost> host,
                         Profile* profile) {
    NormalLaunch(bootstrap, host);
    EXPECT_EQ(host.get(), handler_->FindHost(profile, host->GetAppId()));
    EXPECT_CALL(*handler_, OnShimFocus(host.get(), APP_SHIM_FOCUS_NORMAL, _));
    handler_->OnAppActivated(profile, host->GetAppId());
    EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, host->GetAppLaunchResult());
  }

  // Simulates a focus request coming from a running app shim.
  void ShimNormalFocus(TestHost* host) {
    EXPECT_CALL(*handler_, OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, _))
        .WillOnce(Invoke(handler_.get(),
                         &TestingExtensionAppShimHandler::RealOnShimFocus));

    const std::vector<base::FilePath> no_files;
    handler_->OnShimFocus(host, APP_SHIM_FOCUS_NORMAL, no_files);
  }

  // Simulates a hide (or unhide) request coming from a running app shim.
  void ShimSetHidden(TestHost* host, bool hidden) {
    handler_->OnShimSetHidden(host, hidden);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  MockDelegate* delegate_;
  std::unique_ptr<TestingExtensionAppShimHandler> handler_;
  base::FilePath profile_path_a_;
  base::FilePath profile_path_b_;
  TestingProfile profile_a_;
  TestingProfile profile_b_;

  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_ab_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_bb_;
  base::WeakPtr<TestingAppShimHostBootstrap> bootstrap_aa_duplicate_;

  base::Optional<apps::AppShimLaunchResult> bootstrap_aa_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_ab_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_bb_result_;
  base::Optional<apps::AppShimLaunchResult> bootstrap_aa_duplicate_result_;

  base::WeakPtr<TestHost> host_aa_;
  base::WeakPtr<TestHost> host_ab_;
  base::WeakPtr<TestHost> host_bb_;
  base::WeakPtr<TestHost> host_aa_duplicate_;

  scoped_refptr<const Extension> extension_a_;
  scoped_refptr<const Extension> extension_b_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppShimHandlerTest);
};

TEST_F(ExtensionAppShimHandlerTest, LaunchProfileNotFound) {
  // Bad profile path.
  EXPECT_CALL(*delegate_, ProfileExistsForPath(profile_path_a_))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_PROFILE_NOT_FOUND, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchProfileIsLocked) {
  // Profile is locked.
  EXPECT_CALL(*delegate_, IsProfileLockedForPath(profile_path_a_))
      .WillOnce(Return(true));
  EXPECT_CALL(*delegate_, LaunchUserManager());
  NormalLaunch(bootstrap_aa_, nullptr);
  EXPECT_EQ(APP_SHIM_LAUNCH_PROFILE_LOCKED, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAppNotFound) {
  // App not found.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(static_cast<const Extension*>(NULL)));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(WithArgs<2>(Invoke(delegate_, &MockDelegate::RunCallback)));
  NormalLaunch(bootstrap_aa_, host_aa_);
  EXPECT_EQ(APP_SHIM_LAUNCH_APP_NOT_FOUND, *bootstrap_aa_result_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAppNotEnabled) {
  // App not found.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillOnce(Return(static_cast<const Extension*>(NULL)))
      .WillRepeatedly(Return(extension_a_.get()));
  EXPECT_CALL(*delegate_, EnableExtension(&profile_a_, kTestAppIdA, _))
      .WillOnce(WithArgs<2>(Invoke(delegate_, &MockDelegate::RunCallback)));
  NormalLaunch(bootstrap_aa_, host_aa_);
}

TEST_F(ExtensionAppShimHandlerTest, LaunchAndCloseShim) {
  // Normal startup.
  NormalLaunch(bootstrap_aa_, host_aa_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  NormalLaunch(bootstrap_ab_, host_ab_);
  EXPECT_EQ(host_ab_.get(), handler_->FindHost(&profile_a_, kTestAppIdB));

  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_b_, extension_b_.get(), some_file));
  DoShimLaunch(bootstrap_bb_, host_bb_, APP_SHIM_LAUNCH_NORMAL, some_file);
  EXPECT_EQ(host_bb_.get(), handler_->FindHost(&profile_b_, kTestAppIdB));

  // Activation when there is a registered shim finishes launch with success and
  // focuses the app.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_NORMAL, _));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);

  // Starting and closing a second host just focuses the original host of the
  // app.
  EXPECT_CALL(*handler_,
              OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, some_file));

  DoShimLaunch(bootstrap_aa_duplicate_, host_aa_duplicate_,
               APP_SHIM_LAUNCH_NORMAL, some_file);
  EXPECT_EQ(APP_SHIM_LAUNCH_DUPLICATE_HOST, *bootstrap_aa_duplicate_result_);

  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
  handler_->OnShimClose(host_aa_duplicate_.get());
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Normal close.
  handler_->OnShimClose(host_aa_.get());
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Closing the second host afterward does nothing.
  handler_->OnShimClose(host_aa_duplicate_.get());
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTest, AppLifetime) {
  // When the app activates, if there is no shim, start one.
  EXPECT_CALL(*delegate_, LaunchShim(&profile_a_, extension_a_.get()));
  handler_->OnAppActivated(&profile_a_, kTestAppIdA);

  // Normal shim launch adds an entry in the map.
  // App should not be launched here, but return success to the shim.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, host_aa_);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Return no app windows for OnShimFocus and OnShimQuit.
  AppWindowList app_window_list;
  EXPECT_CALL(*delegate_, GetWindows(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(app_window_list));

  // Non-reopen focus does nothing.
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  ShimNormalFocus(host_aa_.get());

  // Reopen focus launches the app.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, _))
      .WillOnce(Invoke(handler_.get(),
                       &TestingExtensionAppShimHandler::RealOnShimFocus));
  std::vector<base::FilePath> some_file(1, base::FilePath("some_file"));
  EXPECT_CALL(*delegate_,
              LaunchApp(&profile_a_, extension_a_.get(), some_file));
  handler_->OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_REOPEN, some_file);

  // Quit just closes all the windows. This tests that it doesn't terminate,
  // but we expect closing all windows triggers a OnAppDeactivated from
  // AppLifetimeMonitor.
  handler_->OnShimQuit(host_aa_.get());

  // Closing all windows closes the shim and checks if Chrome should be
  // terminated.
  EXPECT_CALL(*delegate_, MaybeTerminate())
      .WillOnce(Return());
  EXPECT_NE(nullptr, host_aa_.get());
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdA);
  EXPECT_EQ(nullptr, host_aa_.get());
}

TEST_F(ExtensionAppShimHandlerTest, MaybeTerminate) {
  // Launch shims, adding entries in the map.
  RegisterOnlyLaunch(bootstrap_aa_, host_aa_);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  RegisterOnlyLaunch(bootstrap_ab_, host_ab_);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_ab_result_);
  EXPECT_EQ(host_ab_.get(), handler_->FindHost(&profile_a_, kTestAppIdB));

  // Return empty window list.
  AppWindowList app_window_list;
  EXPECT_CALL(*delegate_, GetWindows(_, _))
      .WillRepeatedly(Return(app_window_list));

  // Quitting when there's another shim should not terminate.
  EXPECT_CALL(*delegate_, MaybeTerminate())
      .Times(0);
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdA);

  // Quitting when it's the last shim should terminate.
  EXPECT_CALL(*delegate_, MaybeTerminate());
  handler_->OnAppDeactivated(&profile_a_, kTestAppIdB);
}

TEST_F(ExtensionAppShimHandlerTest, RegisterOnly) {
  // For an APP_SHIM_LAUNCH_REGISTER_ONLY, don't launch the app.
  EXPECT_CALL(*delegate_, LaunchApp(_, _, _))
      .Times(0);
  RegisterOnlyLaunch(bootstrap_aa_, host_aa_);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_TRUE(handler_->FindHost(&profile_a_, kTestAppIdA));

  // Close the shim, removing the entry in the map.
  handler_->OnShimClose(host_aa_.get());
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

TEST_F(ExtensionAppShimHandlerTest, LoadProfile) {
  // If the profile is not loaded when an OnShimLaunch arrives, return false
  // and load the profile asynchronously. Launch the app when the profile is
  // ready.
  EXPECT_CALL(*delegate_, ProfileForPath(profile_path_a_))
      .WillOnce(Return(static_cast<Profile*>(NULL)))
      .WillRepeatedly(Return(&profile_a_));
  NormalLaunch(bootstrap_aa_, host_aa_);
  EXPECT_FALSE(handler_->FindHost(&profile_a_, kTestAppIdA));
  delegate_->RunLoadProfileCallback(profile_path_a_, &profile_a_);
  EXPECT_TRUE(handler_->FindHost(&profile_a_, kTestAppIdA));
}

// Tests that calls to OnShimFocus, OnShimHide correctly handle a null extension
// being provided by the extension system.
TEST_F(ExtensionAppShimHandlerTest, ExtensionUninstalled) {
  LaunchAndActivate(bootstrap_aa_, host_aa_, &profile_a_);

  // Have GetWindows() return an empty window list for focus (otherwise, it
  // will contain a single nullptr, which can't be focused). Expect 1 call only.
  AppWindowList empty_window_list;
  EXPECT_CALL(*delegate_, GetWindows(_, _)).WillOnce(Return(empty_window_list));

  ShimNormalFocus(host_aa_.get());
  EXPECT_NE(nullptr, host_aa_.get());

  // Set up the mock to return a null extension, as if it were uninstalled.
  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_a_, kTestAppIdA))
      .WillRepeatedly(Return(nullptr));

  // Now trying to focus should automatically close the shim, and not try to
  // get the window list.
  ShimNormalFocus(host_aa_.get());
  EXPECT_EQ(nullptr, host_aa_.get());

  // Do the same for SetHidden on host_bb.
  LaunchAndActivate(bootstrap_bb_, host_bb_, &profile_b_);
  ShimSetHidden(host_bb_.get(), true);
  EXPECT_NE(nullptr, host_bb_.get());

  EXPECT_CALL(*delegate_, MaybeGetAppExtension(&profile_b_, kTestAppIdB))
      .WillRepeatedly(Return(nullptr));
  ShimSetHidden(host_bb_.get(), true);
  EXPECT_EQ(nullptr, host_bb_.get());
}

TEST_F(ExtensionAppShimHandlerTest, PreExistingHost) {
  // Create a host for our profile.
  delegate_->SetHostForCreate(host_aa_.get());
  EXPECT_EQ(nullptr, handler_->FindHost(&profile_a_, kTestAppIdA));
  EXPECT_EQ(host_aa_.get(),
            handler_->FindOrCreateHost(&profile_a_, kTestAppIdA));
  EXPECT_FALSE(bool(host_aa_->app_launch_result));

  // Launch the app for this host. It should find the pre-existing host, and the
  // pre-existing host's launch result should be set.
  EXPECT_CALL(*handler_, OnShimFocus(host_aa_.get(), APP_SHIM_FOCUS_NORMAL, _))
      .Times(1);
  EXPECT_CALL(*delegate_, LaunchApp(&profile_a_, extension_a_.get(), _))
      .Times(0);
  EXPECT_FALSE(bool(host_aa_->app_launch_result));
  DoShimLaunch(bootstrap_aa_, nullptr, APP_SHIM_LAUNCH_REGISTER_ONLY,
               std::vector<base::FilePath>());
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, host_aa_->app_launch_result);
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, *bootstrap_aa_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));

  // Try to launch the app again. It should fail to launch, and the previous
  // profile should remain.
  DoShimLaunch(bootstrap_aa_duplicate_, nullptr, APP_SHIM_LAUNCH_REGISTER_ONLY,
               std::vector<base::FilePath>());
  EXPECT_EQ(APP_SHIM_LAUNCH_SUCCESS, host_aa_->app_launch_result);
  EXPECT_EQ(APP_SHIM_LAUNCH_DUPLICATE_HOST, *bootstrap_aa_duplicate_result_);
  EXPECT_EQ(host_aa_.get(), handler_->FindHost(&profile_a_, kTestAppIdA));
}

}  // namespace apps
