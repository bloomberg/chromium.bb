// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On Mac, one can't make shortcuts with command-line arguments. Instead, we
// produce small app bundles which locate the Chromium framework and load it,
// passing the appropriate data. This is the entry point into the framework for
// those app bundles.

#import <Cocoa/Cocoa.h>

#include "apps/app_shim/app_shim_messages.h"
#include "base/at_exit.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/mac/app_mode_common.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"

namespace {

const app_mode::ChromeAppModeInfo* g_info;
base::Thread* g_io_thread = NULL;

}  // namespace

// The AppShimController is responsible for communication with the main Chrome
// process, and generally controls the lifetime of the app shim process.
class AppShimController : public IPC::Listener {
 public:
  AppShimController();

  // Connects to Chrome and sends a LaunchApp message.
  void Init();

 private:
  // IPC::Listener implemetation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // If Chrome failed to launch the app, |success| will be false and the app
  // shim process should die.
  void OnLaunchAppDone(bool success);

  // Called when the app is activated, either by the user clicking on it in the
  // dock or by Cmd+Tabbing to it.
  void OnDidActivateApplication();

  // Quits the app shim process.
  void Quit();

  IPC::ChannelProxy* channel_;

  DISALLOW_COPY_AND_ASSIGN(AppShimController);
};

AppShimController::AppShimController() : channel_(NULL) {
}

void AppShimController::Init() {
  DCHECK(g_io_thread);
  base::FilePath user_data_dir;
  if (!PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    Quit();
    return;
  }
  base::FilePath socket_path =
      user_data_dir.Append(app_mode::kAppShimSocketName);
  IPC::ChannelHandle handle(socket_path.value());
  channel_ = new IPC::ChannelProxy(handle, IPC::Channel::MODE_NAMED_CLIENT,
      this, g_io_thread->message_loop_proxy());

  channel_->Send(new AppShimHostMsg_LaunchApp(
      g_info->profile_dir.value(), g_info->app_mode_id));
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
  LOG(ERROR) << "App shim channel error.";
  Quit();
}

void AppShimController::OnLaunchAppDone(bool success) {
  if (!success) {
    Quit();
    return;
  }
  [[[NSWorkspace sharedWorkspace] notificationCenter]
      addObserverForName:NSWorkspaceDidActivateApplicationNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification* notification) {
      NSRunningApplication* activated_app =
          [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];
      if ([activated_app isEqual:[NSRunningApplication currentApplication]])
        OnDidActivateApplication();
  }];
}

void AppShimController::Quit() {
  [NSApp terminate:nil];
}

void AppShimController::OnDidActivateApplication() {
  channel_->Send(new AppShimHostMsg_FocusApp);
}

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

  // Launch the IO thread.
  base::Thread::Options io_thread_options;
  io_thread_options.message_loop_type = MessageLoop::TYPE_IO;
  base::Thread *io_thread = new base::Thread("CrAppShimIO");
  io_thread->StartWithOptions(io_thread_options);
  g_io_thread = io_thread;

  // Launch Chrome if it isn't already running.
  FSRef app_fsref;
  if (!base::mac::FSRefFromPath(info->chrome_outer_bundle_path.value(),
        &app_fsref)) {
    LOG(ERROR) << "base::mac::FSRefFromPath failed for "
               << info->chrome_outer_bundle_path.value();
    return 1;
  }
  std::string silent = std::string("--") + switches::kSilentLaunch;
  CFArrayRef launch_args =
      base::mac::NSToCFCast(@[base::SysUTF8ToNSString(silent)]);

  LSApplicationParameters ls_parameters = {
    0,     // version
    kLSLaunchDefaults,
    &app_fsref,
    NULL,  // asyncLaunchRefCon
    NULL,  // environment
    launch_args,
    NULL   // initialEvent
  };
  ProcessSerialNumber psn;
  // TODO(jeremya): this opens a new browser window if Chrome is already
  // running without any windows open.
  OSStatus status = LSOpenApplication(&ls_parameters, &psn);
  if (status != noErr) {
    OSSTATUS_LOG(ERROR, status) << "LSOpenApplication";
    return 1;
  }

  // This code abuses the fact that Apple Events sent before the process is
  // fully initialized don't receive a reply until its run loop starts. Once
  // the reply is received, Chrome will have opened its IPC port, guaranteed.
  [ReplyEventHandler pingProcess:psn andCall:base::Bind(&OnPingChromeReply)];

  MessageLoopForUI main_message_loop;
  main_message_loop.set_thread_name("MainThread");
  base::PlatformThread::SetName("CrAppShimMain");
  main_message_loop.Run();
  return 0;
}
