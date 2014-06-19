// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <vector>

#include "apps/app_shim/app_shim_handler_mac.h"
#include "apps/app_shim/app_shim_host_manager_mac.h"
#include "apps/app_shim/extension_app_shim_handler_mac.h"
#include "apps/switches.h"
#include "apps/ui/native_app_window.h"
#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/files/file_path_watcher.h"
#include "base/mac/foundation_util.h"
#include "base/mac/launch_services_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_mac.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#import "ui/events/test/cocoa_test_event_utils.h"

namespace {

// General end-to-end test for app shims.
class AppShimInteractiveTest : public extensions::PlatformAppBrowserTest {
 protected:
  AppShimInteractiveTest()
      : auto_reset_(&g_app_shims_allow_update_and_launch_in_tests, true) {}

 private:
  // Temporarily enable app shims.
  base::AutoReset<bool> auto_reset_;

  DISALLOW_COPY_AND_ASSIGN(AppShimInteractiveTest);
};

// Watches for changes to a file. This is designed to be used from the the UI
// thread.
class WindowedFilePathWatcher
    : public base::RefCountedThreadSafe<WindowedFilePathWatcher> {
 public:
  WindowedFilePathWatcher(const base::FilePath& path) : observed_(false) {
    content::BrowserThread::PostTask(
        content::BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&WindowedFilePathWatcher::Watch, this, path));
  }

  void Wait() {
    if (observed_)
      return;

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

 protected:
  friend class base::RefCountedThreadSafe<WindowedFilePathWatcher>;
  virtual ~WindowedFilePathWatcher() {}

  void Watch(const base::FilePath& path) {
    watcher_.Watch(
        path, false, base::Bind(&WindowedFilePathWatcher::Observe, this));
  }

  void Observe(const base::FilePath& path, bool error) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&WindowedFilePathWatcher::StopRunLoop, this));
  }

  void StopRunLoop() {
    observed_ = true;
    if (run_loop_.get())
      run_loop_->Quit();
  }

 private:
  base::FilePathWatcher watcher_;
  bool observed_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowedFilePathWatcher);
};

// Watches for an app shim to connect.
class WindowedAppShimLaunchObserver : public apps::AppShimHandler {
 public:
  WindowedAppShimLaunchObserver(const std::string& app_id)
      : app_mode_id_(app_id),
        observed_(false) {
    apps::AppShimHandler::RegisterHandler(app_id, this);
  }

  void Wait() {
    if (observed_)
      return;

    run_loop_.reset(new base::RunLoop);
    run_loop_->Run();
  }

  // AppShimHandler overrides:
  virtual void OnShimLaunch(Host* host,
                            apps::AppShimLaunchType launch_type,
                            const std::vector<base::FilePath>& files) OVERRIDE {
    // Remove self and pass through to the default handler.
    apps::AppShimHandler::RemoveHandler(app_mode_id_);
    apps::AppShimHandler::GetForAppMode(app_mode_id_)
        ->OnShimLaunch(host, launch_type, files);
    observed_ = true;
    if (run_loop_.get())
      run_loop_->Quit();
  }
  virtual void OnShimClose(Host* host) OVERRIDE {}
  virtual void OnShimFocus(Host* host,
                           apps::AppShimFocusType focus_type,
                           const std::vector<base::FilePath>& files) OVERRIDE {}
  virtual void OnShimSetHidden(Host* host, bool hidden) OVERRIDE {}
  virtual void OnShimQuit(Host* host) OVERRIDE {}

 private:
  std::string app_mode_id_;
  bool observed_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WindowedAppShimLaunchObserver);
};

NSString* GetBundleID(const base::FilePath& shim_path) {
  base::FilePath plist_path = shim_path.Append("Contents").Append("Info.plist");
  NSMutableDictionary* plist = [NSMutableDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(plist_path)];
  return [plist objectForKey:base::mac::CFToNSCast(kCFBundleIdentifierKey)];
}

bool HasAppShimHost(Profile* profile, const std::string& app_id) {
  return g_browser_process->platform_part()
      ->app_shim_host_manager()
      ->extension_app_shim_handler()
      ->FindHost(profile, app_id);
}

}  // namespace

// Watches for NSNotifications from the shared workspace.
@interface WindowedNSNotificationObserver : NSObject {
 @private
  base::scoped_nsobject<NSString> bundleId_;
  BOOL notificationReceived_;
  scoped_ptr<base::RunLoop> runLoop_;
}

- (id)initForNotification:(NSString*)name
              andBundleId:(NSString*)bundleId;
- (void)observe:(NSNotification*)notification;
- (void)wait;
@end

@implementation WindowedNSNotificationObserver

- (id)initForNotification:(NSString*)name
              andBundleId:(NSString*)bundleId {
  if (self = [super init]) {
    bundleId_.reset([[bundleId copy] retain]);
    [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserver:self
           selector:@selector(observe:)
               name:name
             object:nil];
  }
  return self;
}

- (void)observe:(NSNotification*)notification {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NSRunningApplication* application =
      [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];
  if (![[application bundleIdentifier] isEqualToString:bundleId_])
    return;

  [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
  notificationReceived_ = YES;
  if (runLoop_.get())
    runLoop_->Quit();
}

- (void)wait {
  if (notificationReceived_)
    return;

  runLoop_.reset(new base::RunLoop);
  runLoop_->Run();
}

@end

namespace apps {

// Shims require static libraries http://crbug.com/386024.
#if defined(COMPONENT_BUILD)
#define MAYBE_Launch DISABLED_Launch
#define MAYBE_RebuildShim DISABLED_RebuildShim
#else
#define MAYBE_Launch Launch
#define MAYBE_RebuildShim RebuildShim
#endif

// Test that launching the shim for an app starts the app, and vice versa.
// These two cases are combined because the time to run the test is dominated
// by loading the extension and creating the shim.
IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_Launch) {
  // Install the app.
  const extensions::Extension* app = InstallPlatformApp("minimal");

  // Use a WebAppShortcutCreator to get the path.
  web_app::WebAppShortcutCreator shortcut_creator(
      web_app::GetWebAppDataDirectory(profile()->GetPath(), app->id(), GURL()),
      web_app::ShortcutInfoForExtensionAndProfile(app, profile()),
      extensions::FileHandlersInfo());
  base::FilePath shim_path = shortcut_creator.GetInternalShortcutPath();
  EXPECT_FALSE(base::PathExists(shim_path));

  // Create the internal app shim by simulating an app update. FilePathWatcher
  // is used to wait for file operations on the shim to be finished before
  // attempting to launch it. Since all of the file operations are done in the
  // same event on the FILE thread, everything will be done by the time the
  // watcher's callback is executed.
  scoped_refptr<WindowedFilePathWatcher> file_watcher =
      new WindowedFilePathWatcher(shim_path);
  web_app::UpdateAllShortcuts(base::string16(), profile(), app);
  file_watcher->Wait();
  NSString* bundle_id = GetBundleID(shim_path);

  // Case 1: Launch the shim, it should start the app.
  {
    ExtensionTestMessageListener launched_listener("Launched", false);
    CommandLine shim_cmdline(CommandLine::NO_PROGRAM);
    shim_cmdline.AppendSwitch(app_mode::kLaunchedForTest);
    ProcessSerialNumber shim_psn;
    ASSERT_TRUE(base::mac::OpenApplicationWithPath(
        shim_path, shim_cmdline, kLSLaunchDefaults, &shim_psn));
    ASSERT_TRUE(launched_listener.WaitUntilSatisfied());

    ASSERT_TRUE(GetFirstAppWindow());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));

    // If the window is closed, the shim should quit.
    pid_t shim_pid;
    EXPECT_EQ(noErr, GetProcessPID(&shim_psn, &shim_pid));
    GetFirstAppWindow()->GetBaseWindow()->Close();
    ASSERT_TRUE(
        base::WaitForSingleProcess(shim_pid, TestTimeouts::action_timeout()));

    EXPECT_FALSE(GetFirstAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }

  // Case 2: Launch the app, it should start the shim.
  {
    base::scoped_nsobject<WindowedNSNotificationObserver> ns_observer;
    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForNotification:NSWorkspaceDidLaunchApplicationNotification
                andBundleId:bundle_id]);
    WindowedAppShimLaunchObserver observer(app->id());
    LaunchPlatformApp(app);
    [ns_observer wait];
    observer.Wait();

    EXPECT_TRUE(GetFirstAppWindow());
    EXPECT_TRUE(HasAppShimHost(profile(), app->id()));

    // Quitting the shim will eventually cause it to quit. It actually
    // intercepts the -terminate, sends an AppShimHostMsg_QuitApp to Chrome,
    // and returns NSTerminateLater. Chrome responds by closing all windows of
    // the app. Once all windows are closed, Chrome closes the IPC channel,
    // which causes the shim to actually terminate.
    NSArray* running_shim = [NSRunningApplication
        runningApplicationsWithBundleIdentifier:bundle_id];
    ASSERT_EQ(1u, [running_shim count]);

    ns_observer.reset([[WindowedNSNotificationObserver alloc]
        initForNotification:NSWorkspaceDidTerminateApplicationNotification
                andBundleId:bundle_id]);
    [base::mac::ObjCCastStrict<NSRunningApplication>(
        [running_shim objectAtIndex:0]) terminate];
    [ns_observer wait];

    EXPECT_FALSE(GetFirstAppWindow());
    EXPECT_FALSE(HasAppShimHost(profile(), app->id()));
  }
}

#if defined(ARCH_CPU_64_BITS)

// Tests that a 32 bit shim attempting to launch 64 bit Chrome will eventually
// be rebuilt.
IN_PROC_BROWSER_TEST_F(AppShimInteractiveTest, MAYBE_RebuildShim) {
  // Get the 32 bit shim.
  base::FilePath test_data_dir;
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath shim_path_32 =
      test_data_dir.Append("app_shim").Append("app_shim_32_bit.app");
  EXPECT_TRUE(base::PathExists(shim_path_32));

  // Install test app.
  const extensions::Extension* app = InstallPlatformApp("minimal");

  // Use WebAppShortcutCreator to create a 64 bit shim.
  web_app::WebAppShortcutCreator shortcut_creator(
      web_app::GetWebAppDataDirectory(profile()->GetPath(), app->id(), GURL()),
      web_app::ShortcutInfoForExtensionAndProfile(app, profile()),
      extensions::FileHandlersInfo());
  shortcut_creator.UpdateShortcuts();
  base::FilePath shim_path = shortcut_creator.GetInternalShortcutPath();
  NSMutableDictionary* plist_64 = [NSMutableDictionary
      dictionaryWithContentsOfFile:base::mac::FilePathToNSString(
          shim_path.Append("Contents").Append("Info.plist"))];

  // Copy 32 bit shim to where it's expected to be.
  // CopyDirectory doesn't seem to work when copying and renaming in one go.
  ASSERT_TRUE(base::DeleteFile(shim_path, true));
  ASSERT_TRUE(base::PathExists(shim_path.DirName()));
  ASSERT_TRUE(base::CopyDirectory(shim_path_32, shim_path.DirName(), true));
  ASSERT_TRUE(base::Move(shim_path.DirName().Append(shim_path_32.BaseName()),
                         shim_path));
  ASSERT_TRUE(base::PathExists(
      shim_path.Append("Contents").Append("MacOS").Append("app_mode_loader")));

  // Fix up the plist so that it matches the installed test app.
  NSString* plist_path = base::mac::FilePathToNSString(
      shim_path.Append("Contents").Append("Info.plist"));
  NSMutableDictionary* plist =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist_path];

  NSArray* keys_to_copy = @[
    base::mac::CFToNSCast(kCFBundleIdentifierKey),
    base::mac::CFToNSCast(kCFBundleNameKey),
    app_mode::kCrAppModeShortcutIDKey,
    app_mode::kCrAppModeUserDataDirKey,
    app_mode::kBrowserBundleIDKey
  ];
  for (NSString* key in keys_to_copy) {
    [plist setObject:[plist_64 objectForKey:key]
              forKey:key];
  }
  [plist writeToFile:plist_path
          atomically:YES];

  base::mac::RemoveQuarantineAttribute(shim_path);

  // Launch the shim, it should start the app and ultimately connect over IPC.
  // This actually happens in multiple launches of the shim:
  // (1) The shim will fail and instead launch Chrome with --app-id so that the
  //     app starts.
  // (2) Chrome launches the shim in response to an app starting, this time the
  //     shim launches Chrome with --app-shim-error, which causes Chrome to
  //     rebuild the shim.
  // (3) After rebuilding, Chrome again launches the shim and expects it to
  //     behave normally.
  ExtensionTestMessageListener launched_listener("Launched", false);
  CommandLine shim_cmdline(CommandLine::NO_PROGRAM);
  ASSERT_TRUE(base::mac::OpenApplicationWithPath(
      shim_path, shim_cmdline, kLSLaunchDefaults, NULL));

  // Wait for the app to start (1). At this point there is no shim host.
  ASSERT_TRUE(launched_listener.WaitUntilSatisfied());
  EXPECT_FALSE(HasAppShimHost(profile(), app->id()));

  // Wait for the rebuilt shim to connect (3). This does not race with the app
  // starting (1) because Chrome only launches the shim (2) after the app
  // starts. Then Chrome must handle --app-shim-error on the UI thread before
  // the shim is rebuilt.
  WindowedAppShimLaunchObserver(app->id()).Wait();

  EXPECT_TRUE(GetFirstAppWindow());
  EXPECT_TRUE(HasAppShimHost(profile(), app->id()));
}

#endif  // defined(ARCH_CPU_64_BITS)

}  // namespace apps
