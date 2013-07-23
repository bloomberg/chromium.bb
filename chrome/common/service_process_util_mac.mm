// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/service_process_util_posix.h"

#import <Foundation/Foundation.h>
#include <launch.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/version.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/mac/launchd.h"

using ::base::FilePathWatcher;

namespace {

#define kServiceProcessSessionType "Aqua"

CFStringRef CopyServiceProcessLaunchDName() {
  base::mac::ScopedNSAutoreleasePool pool;
  NSBundle* bundle = base::mac::FrameworkBundle();
  return CFStringCreateCopy(kCFAllocatorDefault,
                            base::mac::NSToCFCast([bundle bundleIdentifier]));
}

NSString* GetServiceProcessLaunchDLabel() {
  base::scoped_nsobject<NSString> name(
      base::mac::CFToNSCast(CopyServiceProcessLaunchDName()));
  NSString *label = [name stringByAppendingString:@".service_process"];
  base::FilePath user_data_dir;
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

bool GetParentFSRef(const FSRef& child, FSRef* parent) {
  return FSGetCatalogInfo(&child, 0, NULL, NULL, NULL, parent) == noErr;
}

bool RemoveFromLaunchd() {
  // We're killing a file.
  base::ThreadRestrictions::AssertIOAllowed();
  base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
  return Launchd::GetInstance()->DeletePlist(Launchd::User,
                                             Launchd::Agent,
                                             name);
}

class ExecFilePathWatcherCallback {
 public:
  ExecFilePathWatcherCallback() {}
  ~ExecFilePathWatcherCallback() {}

  bool Init(const base::FilePath& path);
  void NotifyPathChanged(const base::FilePath& path, bool error);

 private:
  FSRef executable_fsref_;
};

}  // namespace

NSString* GetServiceProcessLaunchDSocketEnvVar() {
  NSString *label = GetServiceProcessLaunchDLabel();
  NSString *env_var = [label stringByReplacingOccurrencesOfString:@"."
                                                       withString:@"_"];
  env_var = [env_var stringByAppendingString:@"_SOCKET"];
  env_var = [env_var uppercaseString];
  return env_var;
}

// Gets the name of the service process IPC channel.
IPC::ChannelHandle GetServiceProcessChannel() {
  base::mac::ScopedNSAutoreleasePool pool;
  std::string socket_path;
  base::scoped_nsobject<NSDictionary> dictionary(
      base::mac::CFToNSCast(Launchd::GetInstance()->CopyExports()));
  NSString *ns_socket_path =
      [dictionary objectForKey:GetServiceProcessLaunchDSocketEnvVar()];
  if (ns_socket_path) {
    socket_path = base::SysNSStringToUTF8(ns_socket_path);
  }
  return IPC::ChannelHandle(socket_path);
}

bool ForceServiceProcessShutdown(const std::string& /* version */,
                                 base::ProcessId /* process_id */) {
  base::mac::ScopedNSAutoreleasePool pool;
  CFStringRef label = base::mac::NSToCFCast(GetServiceProcessLaunchDLabel());
  CFErrorRef err = NULL;
  bool ret = Launchd::GetInstance()->RemoveJob(label, &err);
  if (!ret) {
    DLOG(ERROR) << "ForceServiceProcessShutdown: " << err << " "
                << base::SysCFStringRefToUTF8(label);
    CFRelease(err);
  }
  return ret;
}

bool GetServiceProcessData(std::string* version, base::ProcessId* pid) {
  base::mac::ScopedNSAutoreleasePool pool;
  CFStringRef label = base::mac::NSToCFCast(GetServiceProcessLaunchDLabel());
  base::scoped_nsobject<NSDictionary> launchd_conf(
      base::mac::CFToNSCast(Launchd::GetInstance()->CopyJobDictionary(label)));
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
          DLOG(ERROR) << "Unable to get version at: "
                      << reinterpret_cast<CFStringRef>(bundle_path);
        }
      } else {
        // The bundle has been deleted out from underneath the registered
        // job.
        DLOG(ERROR) << "Unable to get bundle at: "
                    << reinterpret_cast<CFStringRef>(bundle_path);
      }
    } else {
      DLOG(ERROR) << "Unable to get executable path for service process";
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
  CFErrorRef err = NULL;
  CFDictionaryRef dict =
      Launchd::GetInstance()->CopyDictionaryByCheckingIn(&err);
  if (!dict) {
    DLOG(ERROR) << "ServiceProcess must be launched by launchd. "
                << "CopyLaunchdDictionaryByCheckingIn: " << err;
    CFRelease(err);
    return false;
  }
  state_->launchd_conf_.reset(dict);
  return true;
}

IPC::ChannelHandle ServiceProcessState::GetServiceProcessChannel() {
  DCHECK(state_);
  NSDictionary *ns_launchd_conf = base::mac::CFToNSCast(state_->launchd_conf_);
  NSDictionary* socket_dict =
      [ns_launchd_conf objectForKey:@ LAUNCH_JOBKEY_SOCKETS];
  NSArray* sockets =
      [socket_dict objectForKey:GetServiceProcessLaunchDSocketKey()];
  DCHECK_EQ([sockets count], 1U);
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
  Version service_version(version);
  bool ready = true;
  if (!service_version.IsValid()) {
    ready = false;
  } else {
    chrome::VersionInfo version_info;
    if (!version_info.is_valid()) {
      // Our own version is invalid. This is an error case. Pretend that we
      // are out of date.
      NOTREACHED();
      ready = true;
    }
    else {
      Version running_version(version_info.Version());
      if (!running_version.IsValid()) {
        // Our own version is invalid. This is an error case. Pretend that we
        // are out of date.
        NOTREACHED();
        ready = true;
      } else if (running_version.CompareTo(service_version) > 0) {
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
        @ kServiceProcessSessionType, @ LAUNCH_JOBKEY_LIMITLOADTOSESSIONTYPE,
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
  base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
  base::ScopedCFTypeRef<CFDictionaryRef> plist(
      CreateServiceProcessLaunchdPlist(autorun_command_line_.get(), true));
  return Launchd::GetInstance()->WritePlistToFile(Launchd::User,
                                                  Launchd::Agent,
                                                  name,
                                                  plist);
}

bool ServiceProcessState::RemoveFromAutoRun() {
  return RemoveFromLaunchd();
}

bool ServiceProcessState::StateData::WatchExecutable() {
  base::mac::ScopedNSAutoreleasePool pool;
  NSDictionary* ns_launchd_conf = base::mac::CFToNSCast(launchd_conf_);
  NSString* exe_path = [ns_launchd_conf objectForKey:@ LAUNCH_JOBKEY_PROGRAM];
  if (!exe_path) {
    DLOG(ERROR) << "No " LAUNCH_JOBKEY_PROGRAM;
    return false;
  }

  base::FilePath executable_path =
      base::FilePath([exe_path fileSystemRepresentation]);
  scoped_ptr<ExecFilePathWatcherCallback> callback(
      new ExecFilePathWatcherCallback);
  if (!callback->Init(executable_path)) {
    DLOG(ERROR) << "executable_watcher_.Init " << executable_path.value();
    return false;
  }
  if (!executable_watcher_.Watch(
          executable_path,
          false,
          base::Bind(&ExecFilePathWatcherCallback::NotifyPathChanged,
                     base::Owned(callback.release())))) {
    DLOG(ERROR) << "executable_watcher_.watch " << executable_path.value();
    return false;
  }
  return true;
}

bool ExecFilePathWatcherCallback::Init(const base::FilePath& path) {
  return base::mac::FSRefFromPath(path.value(), &executable_fsref_);
}

void ExecFilePathWatcherCallback::NotifyPathChanged(const base::FilePath& path,
                                                    bool error) {
  if (error) {
    NOTREACHED();  // TODO(darin): Do something smarter?
    return;
  }

  base::mac::ScopedNSAutoreleasePool pool;
  bool needs_shutdown = false;
  bool needs_restart = false;
  bool good_bundle = false;

  FSRef macos_fsref;
  if (GetParentFSRef(executable_fsref_, &macos_fsref)) {
    FSRef contents_fsref;
    if (GetParentFSRef(macos_fsref, &contents_fsref)) {
      FSRef bundle_fsref;
      if (GetParentFSRef(contents_fsref, &bundle_fsref)) {
        base::ScopedCFTypeRef<CFURLRef> bundle_url(
            CFURLCreateFromFSRef(kCFAllocatorDefault, &bundle_fsref));
        if (bundle_url.get()) {
          base::ScopedCFTypeRef<CFBundleRef> bundle(
              CFBundleCreate(kCFAllocatorDefault, bundle_url));
          // Check to see if the bundle still has a minimal structure.
          good_bundle = CFBundleGetIdentifier(bundle) != NULL;
        }
      }
    }
  }
  if (!good_bundle) {
    needs_shutdown = true;
  } else {
    Boolean in_trash;
    OSErr err = FSDetermineIfRefIsEnclosedByFolder(kOnAppropriateDisk,
                                                   kTrashFolderType,
                                                   &executable_fsref_,
                                                   &in_trash);
    if (err == noErr && in_trash) {
      needs_shutdown = true;
    } else {
      bool was_moved = true;
      FSRef path_ref;
      if (base::mac::FSRefFromPath(path.value(), &path_ref)) {
        if (FSCompareFSRefs(&path_ref, &executable_fsref_) == noErr) {
          was_moved = false;
        }
      }
      if (was_moved) {
        needs_restart = true;
      }
    }
  }
  if (needs_shutdown || needs_restart) {
    // First deal with the plist.
    base::ScopedCFTypeRef<CFStringRef> name(CopyServiceProcessLaunchDName());
    if (needs_restart) {
      base::ScopedCFTypeRef<CFMutableDictionaryRef> plist(
          Launchd::GetInstance()->CreatePlistFromFile(
              Launchd::User, Launchd::Agent, name));
      if (plist.get()) {
        NSMutableDictionary* ns_plist = base::mac::CFToNSCast(plist);
        std::string new_path = base::mac::PathFromFSRef(executable_fsref_);
        NSString* ns_new_path = base::SysUTF8ToNSString(new_path);
        [ns_plist setObject:ns_new_path forKey:@ LAUNCH_JOBKEY_PROGRAM];
        base::scoped_nsobject<NSMutableArray> args([[ns_plist
            objectForKey:@LAUNCH_JOBKEY_PROGRAMARGUMENTS] mutableCopy]);
        [args replaceObjectAtIndex:0 withObject:ns_new_path];
        [ns_plist setObject:args forKey:@ LAUNCH_JOBKEY_PROGRAMARGUMENTS];
        if (!Launchd::GetInstance()->WritePlistToFile(Launchd::User,
                                                      Launchd::Agent,
                                                      name,
                                                      plist)) {
          DLOG(ERROR) << "Unable to rewrite plist.";
          needs_shutdown = true;
        }
      } else {
        DLOG(ERROR) << "Unable to read plist.";
        needs_shutdown = true;
      }
    }
    if (needs_shutdown) {
      if (!RemoveFromLaunchd()) {
        DLOG(ERROR) << "Unable to RemoveFromLaunchd.";
      }
    }

    // Then deal with the process.
    CFStringRef session_type = CFSTR(kServiceProcessSessionType);
    if (needs_restart) {
      if (!Launchd::GetInstance()->RestartJob(Launchd::User,
                                              Launchd::Agent,
                                              name,
                                              session_type)) {
        DLOG(ERROR) << "RestartLaunchdJob";
        needs_shutdown = true;
      }
    }
    if (needs_shutdown) {
      CFStringRef label =
          base::mac::NSToCFCast(GetServiceProcessLaunchDLabel());
      CFErrorRef err = NULL;
      if (!Launchd::GetInstance()->RemoveJob(label, &err)) {
        base::ScopedCFTypeRef<CFErrorRef> scoped_err(err);
        DLOG(ERROR) << "RemoveJob " << err;
        // Exiting with zero, so launchd doesn't restart the process.
        exit(0);
      }
    }
  }
}
