// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_shim/app_shim_controller.h"

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app_shim/app_shim_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/ns_view_bridge_factory_impl.h"
#include "content/public/common/ns_view_bridge_factory.mojom.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views_bridge_mac/bridge_factory_impl.h"
#include "ui/views_bridge_mac/mojo/bridge_factory.mojom.h"

AppShimController::AppShimController(
    const app_mode::ChromeAppModeInfo* app_mode_info)
    : app_mode_info_(app_mode_info),
      shim_binding_(this),
      delegate_([[AppShimDelegate alloc] init]),
      launch_app_done_(false),
      ping_chrome_reply_received_(false),
      attention_request_id_(0) {
  // Since AppShimController is created before the main message loop starts,
  // NSApp will not be set, so use sharedApplication.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:delegate_];
}

AppShimController::~AppShimController() {
  // Un-set the delegate since NSApplication does not retain it.
  NSApplication* sharedApplication = [NSApplication sharedApplication];
  [sharedApplication setDelegate:nil];
}

void AppShimController::OnPingChromeReply(bool success) {
  ping_chrome_reply_received_ = true;
  if (!success) {
    [NSApp terminate:nil];
    return;
  }

  Init();
}

void AppShimController::OnPingChromeTimeout() {
  if (!ping_chrome_reply_received_)
    [NSApp terminate:nil];
}

void AppShimController::Init() {
  ui::WindowResizeHelperMac::Get()->Init(base::ThreadTaskRunnerHandle::Get());
  SetUpMenu();

  // Chrome will relaunch shims when relaunching apps.
  [NSApp disableRelaunchOnLogin];

  // The user_data_dir for shims actually contains the app_data_path.
  // I.e. <user_data_dir>/<profile_dir>/Web Applications/_crx_extensionid/
  user_data_dir_ = app_mode_info_->user_data_dir.DirName().DirName().DirName();
  CHECK(!user_data_dir_.empty());

  base::FilePath symlink_path =
      user_data_dir_.Append(app_mode::kAppShimSocketSymlinkName);

  base::FilePath socket_path;
  if (!base::ReadSymbolicLink(symlink_path, &socket_path)) {
    // The path in the user data dir is not a symlink, try connecting directly.
    CreateChannelAndSendLaunchApp(symlink_path);
    return;
  }

  app_mode::VerifySocketPermissions(socket_path);

  CreateChannelAndSendLaunchApp(socket_path);
}

void AppShimController::CreateChannelAndSendLaunchApp(
    const base::FilePath& socket_path) {
  mojo::ScopedMessagePipeHandle message_pipe = mojo_connection_.Connect(
      mojo::NamedPlatformChannel::ConnectToServer(socket_path.value()));
  host_ = chrome::mojom::AppShimHostPtr(
      chrome::mojom::AppShimHostPtrInfo(std::move(message_pipe), 0));

  chrome::mojom::AppShimPtr app_shim_ptr;
  shim_binding_.Bind(mojo::MakeRequest(&app_shim_ptr),
                     ui::WindowResizeHelperMac::Get()->task_runner());
  shim_binding_.set_connection_error_with_reason_handler(
      base::BindOnce(&AppShimController::ChannelError, base::Unretained(this)));

  bool launched_by_chrome = base::CommandLine::ForCurrentProcess()->HasSwitch(
      app_mode::kLaunchedByChromeProcessId);
  apps::AppShimLaunchType launch_type =
      launched_by_chrome ? apps::APP_SHIM_LAUNCH_REGISTER_ONLY
                         : apps::APP_SHIM_LAUNCH_NORMAL;

  [delegate_ setController:this];

  std::vector<base::FilePath> files;
  [delegate_ getFilesToOpenAtStartup:&files];

  host_->LaunchApp(std::move(app_shim_ptr), app_mode_info_->profile_dir,
                   app_mode_info_->app_mode_id, launch_type, files);
}

void AppShimController::SetUpMenu() {
  NSString* title = base::SysUTF16ToNSString(app_mode_info_->app_mode_name);

  // Create a main menu since [NSApp mainMenu] is nil.
  base::scoped_nsobject<NSMenu> main_menu([[NSMenu alloc] initWithTitle:title]);

  // The title of the first item is replaced by OSX with the name of the app and
  // bold styling. Create a dummy item for this and make it hidden.
  NSMenuItem* dummy_item =
      [main_menu addItemWithTitle:title action:nil keyEquivalent:@""];
  base::scoped_nsobject<NSMenu> dummy_submenu(
      [[NSMenu alloc] initWithTitle:title]);
  [dummy_item setSubmenu:dummy_submenu];
  [dummy_item setHidden:YES];

  // Construct an unbolded app menu, to match how it appears in the Chrome menu
  // bar when the app is focused.
  NSMenuItem* item =
      [main_menu addItemWithTitle:title action:nil keyEquivalent:@""];
  base::scoped_nsobject<NSMenu> submenu([[NSMenu alloc] initWithTitle:title]);
  [item setSubmenu:submenu];

  // Add a quit entry.
  NSString* quit_localized_string =
      l10n_util::GetNSStringF(IDS_EXIT_MAC, app_mode_info_->app_mode_name);
  [submenu addItemWithTitle:quit_localized_string
                     action:@selector(terminate:)
              keyEquivalent:@"q"];

  // Add File, Edit, and Window menus. These are just here to make the
  // transition smoother, i.e. from another application to the shim then to
  // Chrome.
  [main_menu addItemWithTitle:l10n_util::GetNSString(IDS_FILE_MENU_MAC)
                       action:nil
                keyEquivalent:@""];
  [main_menu addItemWithTitle:l10n_util::GetNSString(IDS_EDIT_MENU_MAC)
                       action:nil
                keyEquivalent:@""];
  [main_menu addItemWithTitle:l10n_util::GetNSString(IDS_WINDOW_MENU_MAC)
                       action:nil
                keyEquivalent:@""];

  [NSApp setMainMenu:main_menu];
}

void AppShimController::ChannelError(uint32_t custom_reason,
                                     const std::string& description) {
  LOG(ERROR) << "Channel error custom_reason:" << custom_reason
             << " description: " << description;
  Close();
}

void AppShimController::LaunchAppDone(apps::AppShimLaunchResult result) {
  if (result != apps::APP_SHIM_LAUNCH_SUCCESS) {
    Close();
    return;
  }

  std::vector<base::FilePath> files;
  if ([delegate_ getFilesToOpenAtStartup:&files])
    SendFocusApp(apps::APP_SHIM_FOCUS_OPEN_FILES, files);

  launch_app_done_ = true;
}

void AppShimController::CreateViewsBridgeFactory(
    views_bridge_mac::mojom::BridgeFactoryAssociatedRequest request) {
  views_bridge_mac::BridgeFactoryImpl::Get()->BindRequest(std::move(request));
}

void AppShimController::CreateContentNSViewBridgeFactory(
    content::mojom::NSViewBridgeFactoryAssociatedRequest request) {
  content::NSViewBridgeFactoryImpl::Get()->BindRequest(std::move(request));
}

void AppShimController::Hide() {
  [NSApp hide:nil];
}

void AppShimController::UnhideWithoutActivation() {
  [NSApp unhideWithoutActivation];
}

void AppShimController::SetUserAttention(
    apps::AppShimAttentionType attention_type) {
  switch (attention_type) {
    case apps::APP_SHIM_ATTENTION_CANCEL:
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
      break;
    case apps::APP_SHIM_ATTENTION_CRITICAL:
      attention_request_id_ = [NSApp requestUserAttention:NSCriticalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_INFORMATIONAL:
      attention_request_id_ =
          [NSApp requestUserAttention:NSInformationalRequest];
      break;
    case apps::APP_SHIM_ATTENTION_NUM_TYPES:
      NOTREACHED();
  }
}

void AppShimController::Close() {
  [delegate_ terminateNow];
}

bool AppShimController::SendFocusApp(apps::AppShimFocusType focus_type,
                                     const std::vector<base::FilePath>& files) {
  if (launch_app_done_) {
    host_->FocusApp(focus_type, files);
    return true;
  }

  return false;
}
