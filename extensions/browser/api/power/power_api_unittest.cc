// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/power_api.h"

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/power_save_blocker.h"
#include "extensions/browser/api/power/power_api_manager.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/extension.h"
#include "extensions/common/test_util.h"

namespace extensions {

namespace {

// Args commonly passed to PowerSaveBlockerStubManager::CallFunction().
const char kDisplayArgs[] = "[\"display\"]";
const char kSystemArgs[] = "[\"system\"]";
const char kEmptyArgs[] = "[]";

// Different actions that can be performed as a result of a
// PowerSaveBlocker being created or destroyed.
enum Request {
  BLOCK_APP_SUSPENSION,
  UNBLOCK_APP_SUSPENSION,
  BLOCK_DISPLAY_SLEEP,
  UNBLOCK_DISPLAY_SLEEP,
  // Returned by PowerSaveBlockerStubManager::PopFirstRequest() when no
  // requests are present.
  NONE,
};

// Stub implementation of content::PowerSaveBlocker that just runs a
// callback on destruction.
class PowerSaveBlockerStub : public content::PowerSaveBlocker {
 public:
  explicit PowerSaveBlockerStub(base::Closure unblock_callback)
      : unblock_callback_(unblock_callback) {
  }

  virtual ~PowerSaveBlockerStub() {
    unblock_callback_.Run();
  }

 private:
  base::Closure unblock_callback_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockerStub);
};

// Manages PowerSaveBlockerStub objects.  Tests can instantiate this class
// to make PowerApiManager's calls to create PowerSaveBlockers record the
// actions that would've been performed instead of actually blocking and
// unblocking power management.
class PowerSaveBlockerStubManager {
 public:
  explicit PowerSaveBlockerStubManager(content::BrowserContext* context)
      : browser_context_(context),
        weak_ptr_factory_(this) {
    // Use base::Unretained since callbacks with return values can't use
    // weak pointers.
    PowerApiManager::Get(browser_context_)->SetCreateBlockerFunctionForTesting(
        base::Bind(&PowerSaveBlockerStubManager::CreateStub,
                   base::Unretained(this)));
  }

  ~PowerSaveBlockerStubManager() {
    PowerApiManager::Get(browser_context_)->SetCreateBlockerFunctionForTesting(
        PowerApiManager::CreateBlockerFunction());
  }

  // Removes and returns the first item from |requests_|.  Returns NONE if
  // |requests_| is empty.
  Request PopFirstRequest() {
    if (requests_.empty())
      return NONE;

    Request request = requests_.front();
    requests_.pop_front();
    return request;
  }

 private:
  // Creates a new PowerSaveBlockerStub of type |type|.
  scoped_ptr<content::PowerSaveBlocker> CreateStub(
      content::PowerSaveBlocker::PowerSaveBlockerType type,
      const std::string& reason) {
    Request unblock_request = NONE;
    switch (type) {
      case content::PowerSaveBlocker::kPowerSaveBlockPreventAppSuspension:
        requests_.push_back(BLOCK_APP_SUSPENSION);
        unblock_request = UNBLOCK_APP_SUSPENSION;
        break;
      case content::PowerSaveBlocker::kPowerSaveBlockPreventDisplaySleep:
        requests_.push_back(BLOCK_DISPLAY_SLEEP);
        unblock_request = UNBLOCK_DISPLAY_SLEEP;
        break;
    }
    return scoped_ptr<content::PowerSaveBlocker>(
        new PowerSaveBlockerStub(
            base::Bind(&PowerSaveBlockerStubManager::AppendRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       unblock_request)));
  }

  void AppendRequest(Request request) {
    requests_.push_back(request);
  }

  content::BrowserContext* browser_context_;

  // Requests in chronological order.
  std::deque<Request> requests_;

  base::WeakPtrFactory<PowerSaveBlockerStubManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PowerSaveBlockerStubManager);
};

}  // namespace

class PowerApiTest : public ApiUnitTest {
 public:
  virtual void SetUp() OVERRIDE {
    ApiUnitTest::SetUp();
    manager_.reset(new PowerSaveBlockerStubManager(browser_context()));
  }

  virtual void TearDown() OVERRIDE {
    manager_.reset();
    ApiUnitTest::TearDown();
  }

 protected:
  // Shorthand for PowerRequestKeepAwakeFunction and
  // PowerReleaseKeepAwakeFunction.
  enum FunctionType {
    REQUEST,
    RELEASE,
  };

  // Calls the function described by |type| with |args|, a JSON list of
  // arguments, on behalf of |extension|.
  bool CallFunction(FunctionType type,
                    const std::string& args,
                    const extensions::Extension* extension) {
    scoped_refptr<UIThreadExtensionFunction> function(
        type == REQUEST ?
        static_cast<UIThreadExtensionFunction*>(
            new PowerRequestKeepAwakeFunction) :
        static_cast<UIThreadExtensionFunction*>(
            new PowerReleaseKeepAwakeFunction));
    function->set_extension(extension);
    return api_test_utils::RunFunction(function.get(), args, browser_context());
  }

  // Send a notification to PowerApiManager saying that |extension| has
  // been unloaded.
  void UnloadExtension(const extensions::Extension* extension) {
    PowerApiManager::Get(browser_context())->OnExtensionUnloaded(
        browser_context(), extension, UnloadedExtensionInfo::REASON_UNINSTALL);
  }

  scoped_ptr<PowerSaveBlockerStubManager> manager_;
};

TEST_F(PowerApiTest, RequestAndRelease) {
  // Simulate an extension making and releasing a "display" request and a
  // "system" request.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerApiTest, RequestWithoutRelease) {
  // Simulate an extension calling requestKeepAwake() without calling
  // releaseKeepAwake().  The override should be automatically removed when
  // the extension is unloaded.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  UnloadExtension(extension());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerApiTest, ReleaseWithoutRequest) {
  // Simulate an extension calling releaseKeepAwake() without having
  // calling requestKeepAwake() earlier.  The call should be ignored.
  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerApiTest, UpgradeRequest) {
  // Simulate an extension calling requestKeepAwake("system") and then
  // requestKeepAwake("display").  When the second call is made, a
  // display-sleep-blocking request should be made before the initial
  // app-suspension-blocking request is released.
  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerApiTest, DowngradeRequest) {
  // Simulate an extension calling requestKeepAwake("display") and then
  // requestKeepAwake("system").  When the second call is made, an
  // app-suspension-blocking request should be made before the initial
  // display-sleep-blocking request is released.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension()));
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  ASSERT_TRUE(CallFunction(RELEASE, kEmptyArgs, extension()));
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

TEST_F(PowerApiTest, MultipleExtensions) {
  // Simulate an extension blocking the display from sleeping.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // Create a second extension that blocks system suspend.  No additional
  // PowerSaveBlocker is needed; the blocker from the first extension
  // already covers the behavior requested by the second extension.
  scoped_refptr<Extension> extension2(test_util::CreateEmptyExtension("id2"));
  ASSERT_TRUE(CallFunction(REQUEST, kSystemArgs, extension2.get()));
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // When the first extension is unloaded, a new app-suspension blocker
  // should be created before the display-sleep blocker is destroyed.
  UnloadExtension(extension());
  EXPECT_EQ(BLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());

  // Make the first extension block display-sleep again.
  ASSERT_TRUE(CallFunction(REQUEST, kDisplayArgs, extension()));
  EXPECT_EQ(BLOCK_DISPLAY_SLEEP, manager_->PopFirstRequest());
  EXPECT_EQ(UNBLOCK_APP_SUSPENSION, manager_->PopFirstRequest());
  EXPECT_EQ(NONE, manager_->PopFirstRequest());
}

}  // namespace extensions
