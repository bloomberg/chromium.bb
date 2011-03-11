// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#import <Foundation/Foundation.h>
#include <launch.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/path_service.h"
#include "base/scoped_nsobject.h"
#include "base/string_util.h"
#include "base/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/common/child_process_host.h"
#include "third_party/GTM/Foundation/GTMServiceManagement.h"

namespace {

NSString* GetServiceProcessLaunchDFileName() {
  NSString *bundle_id = [base::mac::MainAppBundle() bundleIdentifier];
  NSString *label = [bundle_id stringByAppendingPathExtension:@"plist"];
  return label;
}

NSString* GetServiceProcessLaunchDLabel() {
  NSString *bundle_id = [base::mac::MainAppBundle() bundleIdentifier];
  NSString *label = [bundle_id stringByAppendingString:@".service_process"];
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  std::string user_data_dir_path = user_data_dir.value();
  NSString *ns_path = base::SysUTF8ToNSString(user_data_dir_path);
  ns_path = [ns_path stringByReplacingOccurrencesOfString:@" "
                                               withString:@"_"];
  label = [label stringByAppendingString:ns_path];
  return label;
}

NSString* GetServiceProcessLaunchDSocketKey() {
  return @"ServiceProcessSocket";
}

NSString* GetServiceProcessLaunchDSocketEnvVar() {
  NSString *label = GetServiceProcessLaunchDLabel();
  NSString *env_var = [label stringByReplacingOccurrencesOfString:@"."
                                                       withString:@"_"];
  env_var = [env_var stringByAppendingString:@"_SOCKET"];
  env_var = [env_var uppercaseString];
  return env_var;
}

// Creates the path that it returns. Must be called on the FILE thread.
NSURL* GetUserAgentPath() {
  NSArray* library_paths = NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, true);
  DCHECK_EQ([library_paths count], 1U);
  NSString* library_path = [library_paths objectAtIndex:0];
  NSString* launch_agents_path =
      [library_path stringByAppendingPathComponent:@"LaunchAgents"];

  NSError* err;
  if (![[NSFileManager defaultManager] createDirectoryAtPath:launch_agents_path
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:&err]) {
    LOG(ERROR) << "GetUserAgentPath: " << err;
  }

  NSString* plist_file_path =
      [launch_agents_path
           stringByAppendingPathComponent:GetServiceProcessLaunchDFileName()];
  return [NSURL fileURLWithPath:plist_file_path isDirectory:NO];
}

}

// Gets the name of the service process IPC channel.
IPC::ChannelHandle GetServiceProcessChannel() {
  std::string socket_path;
  scoped_nsobject<NSDictionary> dictionary(
      base::mac::CFToNSCast(GTMCopyLaunchdExports()));
  NSString *ns_socket_path =
      [dictionary objectForKey:GetServiceProcessLaunchDSocketEnvVar()];
  if (ns_socket_path) {
    socket_path = base::SysNSStringToUTF8(ns_socket_path);
  }
  return IPC::ChannelHandle(socket_path);
}

bool ForceServiceProcessShutdown(const std::string& /* version */,
                                 base::ProcessId /* process_id */) {
  NSString* label = GetServiceProcessLaunchDLabel();
  CFErrorRef err = NULL;
  bool ret = GTMSMJobRemove(reinterpret_cast<CFStringRef>(label), &err);
  if (!ret) {
    LOG(ERROR) << "ForceServiceProcessShutdown: " << err;
    CFRelease(err);
  }
  return ret;
}

bool GetServiceProcessData(std::string* version, base::ProcessId* pid) {
  CFStringRef label =
      reinterpret_cast<CFStringRef>(GetServiceProcessLaunchDLabel());
  scoped_nsobject<NSDictionary> launchd_conf(
      base::mac::CFToNSCast(GTMSMJobCopyDictionary(label)));
  if (!launchd_conf.get()) {
    return false;
  }
  // Anything past here will return true in that there does appear
  // to be a service process of some sort registered with launchd.
  if (version) {
    *version = "0";
    NSString *exe_path = [launchd_conf objectForKey:@ LAUNCH_JOBKEY_PROGRAM];
    if (exe_path) {
      NSString *bundle_path = [[[exe_path stringByDeletingLastPathComponent]
                                stringByDeletingLastPathComponent]
                               stringByDeletingLastPathComponent];
      NSBundle *bundle = [NSBundle bundleWithPath:bundle_path];
      if (bundle) {
        NSString *ns_version =
            [bundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
        if (ns_version) {
          *version = base::SysNSStringToUTF8(ns_version);
        } else {
          LOG(ERROR) << "Unable to get version at: "
                     << reinterpret_cast<CFStringRef>(bundle_path);
        }
      } else {
        // The bundle has been deleted out from underneath the registered
        // job.
        LOG(ERROR) << "Unable to get bundle at: "
                   << reinterpret_cast<CFStringRef>(bundle_path);
      }
    } else {
      LOG(ERROR) << "Unable to get executable path for service process";
    }
  }
  if (pid) {
    *pid = -1;
    NSNumber* ns_pid = [launchd_conf objectForKey:@ LAUNCH_JOBKEY_PID];
    if (ns_pid) {
     *pid = [ns_pid intValue];
    }
  }
  return true;
}

bool ServiceProcessState::Initialize() {
  if (!CreateState()) {
    return false;
  }
  CFErrorRef err = NULL;
  state_->launchd_conf_.reset(GTMSMJobCheckIn(&err));
  if (!state_->launchd_conf_.get()) {
    LOG(ERROR) << "InitializePlatformState: " << err;
    CFRelease(err);
    return false;
  }
  return true;
}

IPC::ChannelHandle ServiceProcessState::GetServiceProcessChannel() {
  CHECK(state_);
  NSDictionary *ns_launchd_conf = base::mac::CFToNSCast(state_->launchd_conf_);
  NSDictionary* socket_dict =
      [ns_launchd_conf objectForKey:@ LAUNCH_JOBKEY_SOCKETS];
  NSArray* sockets =
      [socket_dict objectForKey:GetServiceProcessLaunchDSocketKey()];
  CHECK_EQ([sockets count], 1U);
  int socket = [[sockets objectAtIndex:0] intValue];
  base::FileDescriptor fd(socket, false);
  return IPC::ChannelHandle(std::string(), fd);
}

bool CheckServiceProcessReady() {
  std::string version;
  pid_t pid;
  if (!GetServiceProcessData(&version, &pid)) {
    return false;
  }
  scoped_ptr<Version> service_version(Version::GetVersionFromString(version));
  bool ready = true;
  if (!service_version.get()) {
    ready = false;
  } else {
    chrome::VersionInfo version_info;
    if (!version_info.is_valid()) {
      // Our own version is invalid. This is an error case. Pretend that we
      // are out of date.
      NOTREACHED() << "Failed to get current file version";
      ready = true;
    }
    else {
      scoped_ptr<Version> running_version(Version::GetVersionFromString(
          version_info.Version()));
      if (!running_version.get()) {
        // Our own version is invalid. This is an error case. Pretend that we
        // are out of date.
        NOTREACHED() << "Failed to parse version info";
        ready = true;
      } else if (running_version->CompareTo(*service_version) > 0) {
        ready = false;
      } else {
        ready = true;
      }
    }
  }
  if (!ready) {
    ForceServiceProcessShutdown(version, pid);
  }
  return ready;
}

CFDictionaryRef CreateServiceProcessLaunchdPlist(CommandLine* cmd_line,
                                                 bool for_auto_launch) {
  base::mac::ScopedNSAutoreleasePool pool;

  NSString *program =
      base::SysUTF8ToNSString(cmd_line->GetProgram().value());

  std::vector<std::string> args = cmd_line->argv();
  NSMutableArray *ns_args = [NSMutableArray arrayWithCapacity:args.size()];

  for (std::vector<std::string>::iterator iter = args.begin();
       iter < args.end();
       ++iter) {
    [ns_args addObject:base::SysUTF8ToNSString(*iter)];
  }

  NSDictionary *socket =
      [NSDictionary dictionaryWithObject:GetServiceProcessLaunchDSocketEnvVar()
                                  forKey:@ LAUNCH_JOBSOCKETKEY_SECUREWITHKEY];
  NSDictionary *sockets =
      [NSDictionary dictionaryWithObject:socket
                                  forKey:GetServiceProcessLaunchDSocketKey()];

  // See the man page for launchd.plist.
  NSMutableDictionary *launchd_plist =
      [[NSMutableDictionary alloc] initWithObjectsAndKeys:
        GetServiceProcessLaunchDLabel(), @ LAUNCH_JOBKEY_LABEL,
        program, @ LAUNCH_JOBKEY_PROGRAM,
        ns_args, @ LAUNCH_JOBKEY_PROGRAMARGUMENTS,
        sockets, @ LAUNCH_JOBKEY_SOCKETS,
        nil];

  if (for_auto_launch) {
    // We want the service process to be able to exit if there are no services
    // enabled. With a value of NO in the SuccessfulExit key, launchd will
    // relaunch the service automatically in any other case than exiting
    // cleanly with a 0 return code.
    NSDictionary *keep_alive =
      [NSDictionary
        dictionaryWithObject:[NSNumber numberWithBool:NO]
                      forKey:@ LAUNCH_JOBKEY_KEEPALIVE_SUCCESSFULEXIT];
    NSDictionary *auto_launchd_plist =
      [[NSDictionary alloc] initWithObjectsAndKeys:
        [NSNumber numberWithBool:YES], @ LAUNCH_JOBKEY_RUNATLOAD,
        keep_alive, @ LAUNCH_JOBKEY_KEEPALIVE,
        @"Background", @ LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE,
        nil];
    [launchd_plist addEntriesFromDictionary:auto_launchd_plist];
  }
  return reinterpret_cast<CFDictionaryRef>(launchd_plist);
}

// Writes the launchd property list into the user's LaunchAgents directory,
// creating that directory if needed. This will cause the service process to be
// auto launched on the next user login.
bool ServiceProcessState::AddToAutoRun() {
  // We're creating directories and writing a file.
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(autorun_command_line_.get());

  base::mac::ScopedNSAutoreleasePool pool;
  scoped_nsobject<NSDictionary> plist(
      base::mac::CFToNSCast(CreateServiceProcessLaunchdPlist(
                                autorun_command_line_.get(), true)));
  NSURL* plist_url = GetUserAgentPath();
  return [plist writeToURL:plist_url atomically:YES];
}

bool ServiceProcessState::RemoveFromAutoRun() {
  // We're killing a file.
  base::ThreadRestrictions::AssertIOAllowed();

  base::mac::ScopedNSAutoreleasePool pool;
  NSURL* plist_url = GetUserAgentPath();
  SInt32 error = 0;
  if (!CFURLDestroyResource(reinterpret_cast<CFURLRef>(plist_url), &error)) {
    LOG(ERROR) << "RemoveFromAutoRun: " << error;
    return false;
  }
  return true;
}
