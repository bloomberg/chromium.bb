// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using IconPurpose = content::Manifest::Icon::IconPurpose;

namespace {

const std::tuple<int, int, IconPurpose> kPrimaryIconParams{144, 144,
                                                           IconPurpose::ANY};

InstallableParams GetManifestParams() {
  InstallableParams params;
  params.check_installable = false;
  params.fetch_valid_primary_icon = false;
  return params;
}

InstallableParams GetWebAppParams() {
  InstallableParams params = GetManifestParams();
  params.ideal_primary_icon_size_in_px = 144;
  params.minimum_primary_icon_size_in_px = 144;
  params.check_installable = true;
  params.fetch_valid_primary_icon = true;
  return params;
}

InstallableParams GetPrimaryIconParams() {
  InstallableParams params = GetManifestParams();
  params.ideal_primary_icon_size_in_px = 144;
  params.minimum_primary_icon_size_in_px = 144;
  params.fetch_valid_primary_icon = true;
  return params;
}

InstallableParams GetPrimaryIconAndBadgeIconParams() {
  InstallableParams params = GetManifestParams();
  params.ideal_primary_icon_size_in_px = 144;
  params.minimum_primary_icon_size_in_px = 144;
  params.fetch_valid_primary_icon = true;
  params.ideal_badge_icon_size_in_px = 72;
  params.minimum_badge_icon_size_in_px = 72;
  params.fetch_valid_badge_icon = true;
  return params;
}

}  // anonymous namespace

// Used only for testing pages with no service workers. This class will dispatch
// a RunLoop::QuitClosure when it begins waiting for a service worker to be
// registered.
class LazyWorkerInstallableManager : public InstallableManager {
 public:
  LazyWorkerInstallableManager(content::WebContents* web_contents,
                               base::Closure quit_closure)
      : InstallableManager(web_contents), quit_closure_(quit_closure) {}
  ~LazyWorkerInstallableManager() override {}

 protected:
  void OnWaitingForServiceWorker() override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  };

 private:
  base::Closure quit_closure_;
};

class CallbackTester {
 public:
  explicit CallbackTester(base::Closure quit_closure)
      : quit_closure_(quit_closure) {}

  void OnDidFinishInstallableCheck(const InstallableData& data) {
    error_code_ = data.error_code;
    manifest_url_ = data.manifest_url;
    manifest_ = data.manifest;
    primary_icon_url_ = data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_.reset(new SkBitmap(*data.primary_icon));
    badge_icon_url_ = data.badge_icon_url;
    if (data.badge_icon)
      badge_icon_.reset(new SkBitmap(*data.badge_icon));
    is_installable_ = data.is_installable;
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  InstallableStatusCode error_code() const { return error_code_; }
  const GURL& manifest_url() const { return manifest_url_; }
  const content::Manifest& manifest() const { return manifest_; }
  const GURL& primary_icon_url() const { return primary_icon_url_; }
  const SkBitmap* primary_icon() const { return primary_icon_.get(); }
  const GURL& badge_icon_url() const { return badge_icon_url_; }
  const SkBitmap* badge_icon() const { return badge_icon_.get(); }
  bool is_installable() const { return is_installable_; }

 private:
  base::Closure quit_closure_;
  InstallableStatusCode error_code_;
  GURL manifest_url_;
  content::Manifest manifest_;
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  GURL badge_icon_url_;
  std::unique_ptr<SkBitmap> badge_icon_;
  bool is_installable_;
};

class NestedCallbackTester {
 public:
  NestedCallbackTester(InstallableManager* manager,
                       const InstallableParams& params,
                       base::Closure quit_closure)
      : manager_(manager), params_(params), quit_closure_(quit_closure) {}

  void Run() {
    manager_->GetData(params_,
                      base::Bind(&NestedCallbackTester::OnDidFinishFirstCheck,
                                 base::Unretained(this)));
  }

  void OnDidFinishFirstCheck(const InstallableData& data) {
    error_code_ = data.error_code;
    manifest_url_ = data.manifest_url;
    manifest_ = data.manifest;
    primary_icon_url_ = data.primary_icon_url;
    if (data.primary_icon)
      primary_icon_.reset(new SkBitmap(*data.primary_icon));
    is_installable_ = data.is_installable;

    manager_->GetData(params_,
                      base::Bind(&NestedCallbackTester::OnDidFinishSecondCheck,
                                 base::Unretained(this)));
  }

  void OnDidFinishSecondCheck(const InstallableData& data) {
    EXPECT_EQ(error_code_, data.error_code);
    EXPECT_EQ(manifest_url_, data.manifest_url);
    EXPECT_EQ(primary_icon_url_, data.primary_icon_url);
    EXPECT_EQ(primary_icon_.get(), data.primary_icon);
    EXPECT_EQ(is_installable_, data.is_installable);
    EXPECT_EQ(manifest_.IsEmpty(), data.manifest.IsEmpty());
    EXPECT_EQ(manifest_.start_url, data.manifest.start_url);
    EXPECT_EQ(manifest_.display, data.manifest.display);
    EXPECT_EQ(manifest_.name, data.manifest.name);
    EXPECT_EQ(manifest_.short_name, data.manifest.short_name);

    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

 private:
  InstallableManager* manager_;
  InstallableParams params_;
  base::Closure quit_closure_;
  InstallableStatusCode error_code_;
  GURL manifest_url_;
  content::Manifest manifest_;
  GURL primary_icon_url_;
  std::unique_ptr<SkBitmap> primary_icon_;
  bool is_installable_;
};

class InstallableManagerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns a test server URL to a page controlled by a service worker with
  // |manifest_url| injected as the manifest tag.
  std::string GetURLOfPageWithServiceWorkerAndManifest(
      const std::string& manifest_url) {
    return "/banners/manifest_test_page.html?manifest=" +
           embedded_test_server()->GetURL(manifest_url).spec();
  }

  void NavigateAndRunInstallableManager(CallbackTester* tester,
                                        const InstallableParams& params,
                                        const std::string& url) {
    GURL test_url = embedded_test_server()->GetURL(url);
    ui_test_utils::NavigateToURL(browser(), test_url);
    RunInstallableManager(tester, params);
  }

  void RunInstallableManager(CallbackTester* tester,
                             const InstallableParams& params) {
    InstallableManager* manager = GetManager();
    manager->GetData(params,
                     base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                                base::Unretained(tester)));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure app banners are disabled in the browser so they do not
    // interfere with the test.
    command_line->AppendSwitch(switches::kDisableAddToShelf);
  }

  InstallableManager* GetManager() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    InstallableManager::CreateForWebContents(web_contents);
    InstallableManager* manager =
        InstallableManager::FromWebContents(web_contents);
    CHECK(manager);

    return manager;
  }

  InstallabilityCheckStatus GetStatus() { return GetManager()->page_status_; }
};

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       ManagerBeginsInEmptyState) {
  // Ensure that the InstallableManager starts off with everything null.
  InstallableManager* manager = GetManager();

  EXPECT_TRUE(manager->manifest().IsEmpty());
  EXPECT_TRUE(manager->manifest_url().is_empty());
  EXPECT_TRUE(manager->icons_.empty());
  EXPECT_FALSE(manager->is_installable());

  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_TRUE(manager->tasks_.empty());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckNoManifest) {
  // Ensure that a page with no manifest returns the appropriate error and with
  // null fields for everything.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/no_manifest_test_page.html");
  run_loop.Run();

  // If there is no manifest, everything should be empty.
  EXPECT_TRUE(tester->manifest().IsEmpty());
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_MANIFEST, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifest404) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_missing.json"));
  run_loop.Run();

  // The installable manager should return a manifest URL even if it 404s.
  // However, the check should fail with a ManifestEmpty error.
  EXPECT_TRUE(tester->manifest().IsEmpty());

  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(MANIFEST_EMPTY, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestOnly) {
  // Verify that asking for just the manifest works as expected.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckInstallableParamsDefaultConstructor) {
  // Verify that using InstallableParams' default constructor is equivalent to
  // just asking for the manifest alone.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  InstallableParams params;
  NavigateAndRunInstallableManager(tester.get(), params,
                                   "/banners/manifest_test_page.html");
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestWithOnlyRelatedApplications) {
  // This page has a manifest with only related applications specified. Asking
  // for just the manifest should succeed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetManifestParams(),
                                     "/banners/play_app_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
  }

  // Ask for a primary icon (but don't navigate). This should fail with
  // NO_ACCEPTABLE_ICON.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetPrimaryIconParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
  }

  // Ask for everything except badge icon. This should fail with
  // NO_ACCEPTABLE_ICON - the primary icon fetch has already failed, so that
  // cached error stops the installable check from being performed.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
  }

  // Do not ask for primary icon. This should fail with START_URL_NOT_VALID.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.fetch_valid_primary_icon = false;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_TRUE(tester->manifest().prefer_related_applications);

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(START_URL_NOT_VALID, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckManifestAndIcon) {
  // Add to homescreen checks for manifest + primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetPrimaryIconParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
  }

  // Add to homescreen checks for manifest + primary icon + badge icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetPrimaryIconAndBadgeIconParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_FALSE(tester->badge_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
  }

  // Request an oversized badge icon. This should fetch only the manifest and
  // the primary icon, and return no errors.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetPrimaryIconAndBadgeIconParams();
    params.ideal_badge_icon_size_in_px = 2000;
    params.minimum_badge_icon_size_in_px = 2000;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
  }

  // Navigate to a page with a bad badge icon. This should now fail with
  // NO_ICON_AVAILABLE, but still have the manifest and primary icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(),
                                     GetPrimaryIconAndBadgeIconParams(),
                                     GetURLOfPageWithServiceWorkerAndManifest(
                                         "/banners/manifest_bad_badge.json"));
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_ICON_AVAILABLE, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebapp) {
  // Request everything except badge icon.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_PROGRESSIVE_WEB_APP);

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(manager->manifest().IsEmpty());
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->is_installable());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE((manager->icon_url(kPrimaryIconParams).is_empty()));
    EXPECT_NE(nullptr, (manager->icon(kPrimaryIconParams)));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
    EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error(kPrimaryIconParams)));
    EXPECT_TRUE(manager->tasks_.empty());
  }

  // Request everything except badge icon again without navigating away. This
  // should work fine.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    RunInstallableManager(tester.get(), GetWebAppParams());
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_PROGRESSIVE_WEB_APP);

    // Verify that the returned state matches manager internal state.
    InstallableManager* manager = GetManager();

    EXPECT_FALSE(manager->manifest().IsEmpty());
    EXPECT_FALSE(manager->manifest_url().is_empty());
    EXPECT_TRUE(manager->is_installable());
    EXPECT_EQ(1u, manager->icons_.size());
    EXPECT_FALSE((manager->icon_url(kPrimaryIconParams).is_empty()));
    EXPECT_NE(nullptr, (manager->icon(kPrimaryIconParams)));
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
    EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error(kPrimaryIconParams)));
    EXPECT_TRUE(manager->tasks_.empty());
  }

  {
    // Check that a subsequent navigation resets state.
    ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
    InstallableManager* manager = GetManager();

    EXPECT_TRUE(manager->manifest().IsEmpty());
    EXPECT_TRUE(manager->manifest_url().is_empty());
    EXPECT_FALSE(manager->is_installable());
    EXPECT_TRUE(manager->icons_.empty());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
    EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
    EXPECT_TRUE(manager->tasks_.empty());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckWebappInIframe) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                   "/banners/iframe_test_page.html");
  run_loop.Run();

  // The installable manager should only retrieve items in the main frame;
  // everything should be empty here.
  EXPECT_TRUE(tester->manifest().IsEmpty());
  EXPECT_TRUE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_MANIFEST, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckPageWithManifestAndNoServiceWorker) {
  // Just fetch the manifest. This should have no error.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(
        tester.get(), GetManifestParams(),
        "/banners/manifest_no_service_worker.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
    EXPECT_EQ(GetStatus(), InstallabilityCheckStatus::NOT_COMPLETED);
  }

  // Fetching the full criteria should fail if we don't wait for the worker.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.wait_for_worker = false;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->manifest_url().is_empty());

    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_MATCHING_SERVICE_WORKER, tester->error_code());
    EXPECT_EQ(GetStatus(),
              InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckLazyServiceWorkerPassesWhenWaiting) {
  base::RunLoop tester_run_loop, sw_run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(tester_run_loop.QuitClosure()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto manager = base::MakeUnique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  // Load a URL with no service worker.
  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Kick off fetching the data. This should block on waiting for a worker.
  manager->GetData(GetWebAppParams(),
                   base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                              base::Unretained(tester.get())));
  sw_run_loop.Run();

  // We should now be waiting for the service worker.
  EXPECT_FALSE(manager->manifest().IsEmpty());
  EXPECT_FALSE(manager->manifest_url().is_empty());
  EXPECT_FALSE(manager->is_installable());
  EXPECT_EQ(1u, manager->icons_.size());
  EXPECT_FALSE((manager->icon_url(kPrimaryIconParams).is_empty()));
  EXPECT_NE(nullptr, (manager->icon(kPrimaryIconParams)));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error(kPrimaryIconParams)));
  EXPECT_TRUE(manager->worker_waiting());
  EXPECT_FALSE(manager->tasks_.empty());

  // Load the service worker.
  EXPECT_TRUE(content::ExecuteScript(
      web_contents, "navigator.serviceWorker.register('service_worker.js');"));
  tester_run_loop.Run();

  // We should have passed now.
  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_TRUE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  EXPECT_EQ(manager->page_status_,
            InstallabilityCheckStatus::COMPLETE_PROGRESSIVE_WEB_APP);

  // Verify internal state.
  EXPECT_FALSE(manager->manifest().IsEmpty());
  EXPECT_FALSE(manager->manifest_url().is_empty());
  EXPECT_TRUE(manager->is_installable());
  EXPECT_EQ(1u, manager->icons_.size());
  EXPECT_FALSE((manager->icon_url(kPrimaryIconParams).is_empty()));
  EXPECT_NE(nullptr, (manager->icon(kPrimaryIconParams)));
  EXPECT_EQ(NO_ERROR_DETECTED, manager->manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->valid_manifest_error());
  EXPECT_EQ(NO_ERROR_DETECTED, manager->worker_error());
  EXPECT_EQ(NO_ERROR_DETECTED, (manager->icon_error(kPrimaryIconParams)));
  EXPECT_FALSE(manager->worker_waiting());
  EXPECT_TRUE(manager->tasks_.empty());
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckLazyServiceWorkerNoFetchHandlerFails) {
  base::RunLoop tester_run_loop, sw_run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(tester_run_loop.QuitClosure()));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto manager = base::MakeUnique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  // Load a URL with no service worker.
  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Kick off fetching the data. This should block on waiting for a worker.
  manager->GetData(GetWebAppParams(),
                   base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                              base::Unretained(tester.get())));
  sw_run_loop.Run();

  // We should now be waiting for the service worker.
  EXPECT_TRUE(manager->worker_waiting());
  EXPECT_FALSE(manager->tasks_.empty());

  // Load the service worker with no fetch handler.
  EXPECT_TRUE(content::ExecuteScript(web_contents,
                                     "navigator.serviceWorker.register('"
                                     "service_worker_no_fetch_handler.js');"));
  tester_run_loop.Run();

  // We should fail the check.
  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NOT_OFFLINE_CAPABLE, tester->error_code());
  EXPECT_FALSE(manager->worker_waiting());
  EXPECT_EQ(manager->page_status_,
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckServiceWorkerErrorIsNotCached) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::RunLoop sw_run_loop;
  auto manager = base::MakeUnique<LazyWorkerInstallableManager>(
      web_contents, sw_run_loop.QuitClosure());

  // Load a URL with no service worker.
  GURL test_url = embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  {
    base::RunLoop tester_run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(tester_run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.wait_for_worker = false;
    manager->GetData(params,
                     base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                                base::Unretained(tester.get())));
    tester_run_loop.Run();

    // We should have returned with an error.
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_EQ(NO_MATCHING_SERVICE_WORKER, tester->error_code());
  }

  {
    base::RunLoop tester_run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(tester_run_loop.QuitClosure()));

    InstallableParams params = GetWebAppParams();
    params.wait_for_worker = true;
    manager->GetData(params,
                     base::Bind(&CallbackTester::OnDidFinishInstallableCheck,
                                base::Unretained(tester.get())));
    sw_run_loop.Run();

    EXPECT_TRUE(content::ExecuteScript(
        web_contents,
        "navigator.serviceWorker.register('service_worker.js');"));
    tester_run_loop.Run();

    // The callback should tell us that the page is installable
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckPageWithNoServiceWorkerFetchHandler) {
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(
      tester.get(), GetWebAppParams(),
      "/banners/no_sw_fetch_handler_test_page.html");

  RunInstallableManager(tester.get(), GetWebAppParams());
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NOT_OFFLINE_CAPABLE, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest, CheckDataUrlIcon) {
  // Verify that InstallableManager can handle data URL icons.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_data_url_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());

  EXPECT_FALSE(tester->primary_icon_url().is_empty());
  EXPECT_NE(nullptr, tester->primary_icon());
  EXPECT_EQ(144, tester->primary_icon()->width());
  EXPECT_TRUE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckManifestCorruptedIcon) {
  // Verify that the returned InstallableData::primary_icon is null if the web
  // manifest points to a corrupt primary icon.
  base::RunLoop run_loop;
  std::unique_ptr<CallbackTester> tester(
      new CallbackTester(run_loop.QuitClosure()));

  NavigateAndRunInstallableManager(tester.get(), GetPrimaryIconParams(),
                                   GetURLOfPageWithServiceWorkerAndManifest(
                                       "/banners/manifest_bad_icon.json"));
  run_loop.Run();

  EXPECT_FALSE(tester->manifest().IsEmpty());
  EXPECT_FALSE(tester->manifest_url().is_empty());
  EXPECT_TRUE(tester->primary_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->primary_icon());
  EXPECT_FALSE(tester->is_installable());
  EXPECT_TRUE(tester->badge_icon_url().is_empty());
  EXPECT_EQ(nullptr, tester->badge_icon());
  EXPECT_EQ(NO_ICON_AVAILABLE, tester->error_code());
  EXPECT_EQ(GetStatus(),
            InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP);
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckChangeInIconDimensions) {
  // Verify that a follow-up request for a primary icon with a different size
  // works.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    NavigateAndRunInstallableManager(tester.get(), GetWebAppParams(),
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // Dial up the primary icon size requirements to something that isn't
    // available. This should now fail with NO_ACCEPTABLE_ICON.
    InstallableParams params = GetWebAppParams();
    params.ideal_primary_icon_size_in_px = 2000;
    params.minimum_primary_icon_size_in_px = 2000;
    RunInstallableManager(tester.get(), params);
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  // Navigate and verify the reverse: an overly large primary icon requested
  // first fails, but a smaller primary icon requested second passes.
  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));

    // This should fail with NO_ACCEPTABLE_ICON.
    InstallableParams params = GetWebAppParams();
    params.ideal_primary_icon_size_in_px = 2000;
    params.minimum_primary_icon_size_in_px = 2000;
    NavigateAndRunInstallableManager(tester.get(), params,
                                     "/banners/manifest_test_page.html");
    run_loop.Run();

    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_TRUE(tester->primary_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->primary_icon());
    EXPECT_FALSE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ACCEPTABLE_ICON, tester->error_code());
  }

  {
    base::RunLoop run_loop;
    std::unique_ptr<CallbackTester> tester(
        new CallbackTester(run_loop.QuitClosure()));
    RunInstallableManager(tester.get(), GetWebAppParams());

    run_loop.Run();

    // The smaller primary icon requirements should allow this to pass.
    EXPECT_FALSE(tester->manifest_url().is_empty());
    EXPECT_FALSE(tester->manifest().IsEmpty());
    EXPECT_FALSE(tester->primary_icon_url().is_empty());
    EXPECT_NE(nullptr, tester->primary_icon());
    EXPECT_TRUE(tester->is_installable());
    EXPECT_TRUE(tester->badge_icon_url().is_empty());
    EXPECT_EQ(nullptr, tester->badge_icon());
    EXPECT_EQ(NO_ERROR_DETECTED, tester->error_code());
  }
}

IN_PROC_BROWSER_TEST_F(InstallableManagerBrowserTest,
                       CheckNestedCallsToGetData) {
  // Verify that we can call GetData while in a callback from GetData.
  base::RunLoop run_loop;
  InstallableParams params = GetWebAppParams();
  std::unique_ptr<NestedCallbackTester> tester(
      new NestedCallbackTester(GetManager(), params, run_loop.QuitClosure()));

  tester->Run();
  run_loop.Run();
}
