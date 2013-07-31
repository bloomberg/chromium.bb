// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>

#include "apps/app_shim/app_shim_messages.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/launch_services_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const app_mode::ChromeAppModeInfo* g_info;
base::Thread* g_io_thread = NULL;

}  // namespace

class AppShimController;

@interface AppShimDelegate : NSObject<NSApplicationDelegate> {
 @private
  AppShimController* appShimController_;  // Weak. Owns us.
  BOOL terminateNow_;
  BOOL terminateRequested_;
}

- (id)initWithController:(AppShimController*)controller;
- (BOOL)applicationOpenUntitledFile:(NSApplication *)app;
- (void)applicationWillBecomeActive:(NSNotification*)notification;
- (void)applicationWillHide:(NSNotification*)notification;
- (void)applicationWillUnhide:(NSNotification*)notification;
- (void)terminateNow;

@end

// The AppShimController is responsible for communication with the main Chrome
// process, and generally controls the lifetime of the app shim process.
class AppShimController : public IPC::Listener {
 public:
  AppShimController();

  // Connects to Chrome and sends a LaunchApp message.
  void Init();

  // Builds main menu bar items.
  void SetUpMenu();

  void SendSetAppHidden(bool hidden);

  void SendQuitApp();

  // Called when the app is activated, either by the user clicking on it in the
  // dock or by Cmd+Tabbing to it.
  void ActivateApp(bool is_reopen);

 private:
  // IPC::Listener implemetation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // If Chrome failed to launch the app, |success| will be false and the app
  // shim process should die.
  void OnLaunchAppDone(apps::AppShimLaunchResult result);

  // Terminates the app shim process.
  void Close();

  IPC::ChannelProxy* channel_;
  base::scoped_nsobject<AppShimDelegate> nsapp_delegate_;
  bool launch_app_done_;

  DISALLOW_COPY_AND_ASSIGN(AppShimController);
};

AppShimController::AppShimController() : channel_(NULL),
                                         launch_app_done_(false) {}

void AppShimController::Init() {
  DCHECK(g_io_thread);

  SetUpMenu();

  // The user_data_dir for shims actually contains the app_data_path.
  // I.e. <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/
  base::FilePath user_data_dir =
      g_info->user_data_dir.DirName().DirName().DirName();
  CHECK(!user_data_dir.empty());

  base::FilePath socket_path =
      user_data_dir.Append(app_mode::kAppShimSocketName);
  IPC::ChannelHandle handle(socket_path.value());
  channel_ = new IPC::ChannelProxy(handle, IPC::Channel::MODE_NAMED_CLIENT,
      this, g_io_thread->message_loop_proxy().get());

  channel_->Send(new AppShimHostMsg_LaunchApp(
      g_info->profile_dir, g_info->app_mode_id,
      CommandLine::ForCurrentProcess()->HasSwitch(app_mode::kNoLaunchApp) ?
          apps::APP_SHIM_LAUNCH_REGISTER_ONLY : apps::APP_SHIM_LAUNCH_NORMAL));

  nsapp_delegate_.reset([[AppShimDelegate alloc] initWithController:this]);
  DCHECK(![NSApp delegate]);
  [NSApp setDelegate:nsapp_delegate_];
}

void AppShimController::SetUpMenu() {
  NSString* title = base::SysUTF16ToNSString(g_info->app_mode_name);

  // Create a main menu since [NSApp mainMenu] is nil.
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:title]);

  // The title of the first item is replaced by OSX with the name of the app and
  // bold styling. Create a dummy item for this and make it hidden.
  NSMenuItem* dummy_item = [main_menu addItemWithTitle:title
                                                action:nil
                                         keyEquivalent:@""];
  base::scoped_nsobject<NSMenu> dummy_submenu(
      [[NSMenu alloc] initWithTitle:title]);
  [dummy_item setSubmenu:dummy_submenu];
  [dummy_item setHidden:YES];

  // Construct an unbolded app menu, to match how it appears in the Chrome menu
  // bar when the app is focused.
  NSMenuItem* item = [main_menu addItemWithTitle:title
                                          action:nil
                                   keyEquivalent:@""];
  base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] initWithTitle:title]);
  [item setSubmenu:submenu];

  // Add a quit entry.
  NSString* quit_localized_string =
      l10n_util::GetNSStringF(IDS_EXIT_MAC, g_info->app_mode_name);
  [submenu addItemWithTitle:quit_localized_string
                     action:@selector(terminate:)
              keyEquivalent:@"q"];

  [NSApp setMainMenu:main_menu];
}

void AppShimController::SendQuitApp() {
  channel_->Send(new AppShimHostMsg_QuitApp);
}

bool AppShimController::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AppShimController, message)
    IPC_MESSAGE_HANDLER(AppShimMsg_LaunchApp_Done, OnLaunchAppDone)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void AppShimController::OnChannelError() {
  Close();
}

void AppShimController::OnLaunchAppDone(apps::AppShimLaunchResult result) {
  if (result != apps::APP_SHIM_LAUNCH_SUCCESS) {
    Close();
    return;
  }

  launch_app_done_ = true;
}

void AppShimController::Close() {
  [nsapp_delegate_ terminateNow];
}

void AppShimController::ActivateApp(bool is_reopen) {
  if (launch_app_done_) {
    channel_->Send(new AppShimHostMsg_FocusApp(
        is_reopen ? apps::APP_SHIM_FOCUS_REOPEN : apps::APP_SHIM_FOCUS_NORMAL));
  }
}

void AppShimController::SendSetAppHidden(bool hidden) {
  channel_->Send(new AppShimHostMsg_SetAppHidden(hidden));
}

@implementation AppShimDelegate

- (id)initWithController:(AppShimController*)controller {
  if ((self = [super init])) {
    appShimController_ = controller;
  }
  return self;
}

- (BOOL)applicationOpenUntitledFile:(NSApplication *)app {
  appShimController_->ActivateApp(true);
  return YES;
}

- (void)applicationWillBecomeActive:(NSNotification*)notification {
  appShimController_->ActivateApp(false);
}

- (NSApplicationTerminateReply)
    applicationShouldTerminate:(NSApplication*)sender {
  if (terminateNow_)
    return NSTerminateNow;

  appShimController_->SendQuitApp();
  // Wait for the channel to close before terminating.
  terminateRequested_ = YES;
  return NSTerminateLater;
}

- (void)applicationWillHide:(NSNotification*)notification {
  appShimController_->SendSetAppHidden(true);
}

- (void)applicationWillUnhide:(NSNotification*)notification {
  appShimController_->SendSetAppHidden(false);
}

- (void)terminateNow {
  if (terminateRequested_) {
    [NSApp replyToApplicationShouldTerminate:NSTerminateNow];
    return;
  }

  terminateNow_ = YES;
  [NSApp terminate:nil];
}

@end

//-----------------------------------------------------------------------------

// A ReplyEventHandler is a helper class to send an Apple Event to a process
// and call a callback when the reply returns.
//
// This is used to 'ping' the main Chrome process -- once Chrome has sent back
// an Apple Event reply, it's guaranteed that it has opened the IPC channel
// that the app shim will connect to.
@interface ReplyEventHandler : NSObject {
  base::Callback<void(bool)> onReply_;
  AEDesc replyEvent_;
}
// Sends an Apple Event to the process identified by |psn|, and calls |replyFn|
// when the reply is received. Internally this creates a ReplyEventHandler,
// which will delete itself once the reply event has been received.
+ (void)pingProcess:(const ProcessSerialNumber&)psn
            andCall:(base::Callback<void(bool)>)replyFn;
@end

@interface ReplyEventHandler (PrivateMethods)
// Initialise the reply event handler. Doesn't register any handlers until
// |-pingProcess:| is called. |replyFn| is the function to be called when the
// Apple Event reply arrives.
- (id)initWithCallback:(base::Callback<void(bool)>)replyFn;

// Sends an Apple Event ping to the process identified by |psn| and registers
// to listen for a reply.
- (void)pingProcess:(const ProcessSerialNumber&)psn;

// Called when a response is received from the target process for the ping sent
// by |-pingProcess:|.
- (void)message:(NSAppleEventDescriptor*)event
      withReply:(NSAppleEventDescriptor*)reply;

// Calls |onReply_|, passing it |success| to specify whether the ping was
// successful.
- (void)closeWithSuccess:(bool)success;
@end

@implementation ReplyEventHandler
+ (void)pingProcess:(const ProcessSerialNumber&)psn
            andCall:(base::Callback<void(bool)>)replyFn {
  // The object will release itself when the reply arrives, or possibly earlier
  // if an unrecoverable error occurs.
  ReplyEventHandler* handler =
      [[ReplyEventHandler alloc] initWithCallback:replyFn];
  [handler pingProcess:psn];
}
@end

@implementation ReplyEventHandler (PrivateMethods)
- (id)initWithCallback:(base::Callback<void(bool)>)replyFn {
  if ((self = [super init])) {
    onReply_ = replyFn;
  }
  return self;
}

- (void)pingProcess:(const ProcessSerialNumber&)psn {
  // Register the reply listener.
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em setEventHandler:self
          andSelector:@selector(message:withReply:)
        forEventClass:'aevt'
           andEventID:'ansr'];
  // Craft the Apple Event to send.
  NSAppleEventDescriptor* target = [NSAppleEventDescriptor
      descriptorWithDescriptorType:typeProcessSerialNumber
                             bytes:&psn
                            length:sizeof(psn)];
  NSAppleEventDescriptor* initial_event =
      [NSAppleEventDescriptor
          appleEventWithEventClass:app_mode::kAEChromeAppClass
                           eventID:app_mode::kAEChromeAppPing
                  targetDescriptor:target
                          returnID:kAutoGenerateReturnID
                     transactionID:kAnyTransactionID];
  // And away we go.
  // TODO(jeremya): if we don't care about the contents of the reply, can we
  // pass NULL for the reply event parameter?
  OSStatus status = AESendMessage(
      [initial_event aeDesc], &replyEvent_, kAEQueueReply, kAEDefaultTimeout);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "AESendMessage";
    [self closeWithSuccess:false];
  }
}

- (void)message:(NSAppleEventDescriptor*)event
      withReply:(NSAppleEventDescriptor*)reply {
  [self closeWithSuccess:true];
}

- (void)closeWithSuccess:(bool)success {
  onReply_.Run(success);
  NSAppleEventManager* em = [NSAppleEventManager sharedAppleEventManager];
  [em removeEventHandlerForEventClass:'aevt' andEventID:'ansr'];
  [self release];
}
@end

//-----------------------------------------------------------------------------

namespace {

// Called when the main Chrome process responds to the Apple Event ping that
// was sent, or when the ping fails (if |success| is false).
void OnPingChromeReply(bool success) {
  if (!success) {
    [NSApp terminate:nil];
    return;
  }
  AppShimController* controller = new AppShimController;
  controller->Init();
}

}  // namespace

extern "C" {

// |ChromeAppModeStart()| is the point of entry into the framework from the app
// mode loader.
__attribute__((visibility("default")))
int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info);

}  // extern "C"

int ChromeAppModeStart(const app_mode::ChromeAppModeInfo* info) {
  CommandLine::Init(info->argc, info->argv);

  base::mac::ScopedNSAutoreleasePool scoped_pool;
  base::AtExitManager exit_manager;
  chrome::RegisterPathProvider();

  if (info->major_version < app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "App Mode Loader too old.");
    return 1;
  }
  if (info->major_version > app_mode::kCurrentChromeAppModeInfoMajorVersion) {
    RAW_LOG(ERROR, "Browser Framework too old to load App Shortcut.");
    return 1;
  }

  g_info = info;

  // Set bundle paths. This loads the bundles.
  base::mac::SetOverrideOuterBundlePath(g_info->chrome_outer_bundle_path);
  base::mac::SetOverrideFrameworkBundlePath(
      g_info->chrome_versioned_path.Append(chrome::kFrameworkName));

  // Calculate the preferred locale used by Chrome.
  // We can't use l10n_util::OverrideLocaleWithCocoaLocale() because it calls
  // [base::mac::OuterBundle() preferredLocalizations] which gets localizations
  // from the bundle of the running app (i.e. it is equivalent to
  // [[NSBundle mainBundle] preferredLocalizations]) instead of the target
  // bundle.
  NSArray* preferred_languages = [NSLocale preferredLanguages];
  NSArray* supported_languages = [base::mac::OuterBundle() localizations];
  std::string preferred_localization;
  for (NSString* language in preferred_languages) {
    if ([supported_languages containsObject:language]) {
      preferred_localization = base::SysNSStringToUTF8(language);
      break;
    }
  }
  std::string locale = l10n_util::NormalizeLocale(
      l10n_util::GetApplicationLocale(preferred_localization));

  // Load localized strings.
  ResourceBundle::InitSharedInstanceLocaleOnly(locale, NULL);

  // Launch the IO thread.
  base::Thread::Options io_thread_options;
  io_thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  base::Thread *io_thread = new base::Thread("CrAppShimIO");
  io_thread->StartWithOptions(io_thread_options);
  g_io_thread = io_thread;

  // Find already running instances of Chrome.
  NSString* chrome_bundle_id = [base::mac::OuterBundle() bundleIdentifier];
  NSArray* existing_chrome = [NSRunningApplication
      runningApplicationsWithBundleIdentifier:chrome_bundle_id];

  // Launch Chrome if it isn't already running.
  ProcessSerialNumber psn;
  if ([existing_chrome count] > 0) {
    OSStatus status = GetProcessForPID(
        [[existing_chrome objectAtIndex:0] processIdentifier], &psn);
    if (status)
      return 1;

  } else {
    CommandLine command_line(CommandLine::NO_PROGRAM);
    command_line.AppendSwitch(switches::kSilentLaunch);
    command_line.AppendSwitchPath(switches::kProfileDirectory,
                                  info->profile_dir);
    bool success =
        base::mac::OpenApplicationWithPath(base::mac::OuterBundlePath(),
                                           command_line,
                                           kLSLaunchDefaults,
                                           &psn);
    if (!success)
      return 1;
  }

  // This code abuses the fact that Apple Events sent before the process is
  // fully initialized don't receive a reply until its run loop starts. Once
  // the reply is received, Chrome will have opened its IPC port, guaranteed.
  [ReplyEventHandler pingProcess:psn andCall:base::Bind(&OnPingChromeReply)];

  base::MessageLoopForUI main_message_loop;
  main_message_loop.set_thread_name("MainThread");
  base::PlatformThread::SetName("CrAppShimMain");
  main_message_loop.Run();
  return 0;
}
