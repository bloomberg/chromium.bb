// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "ui/views/win/hwnd_util.h"
#endif

namespace extensions {

namespace {

#if defined(OS_WIN)
// Name of the command line switch used to pass handle of the native view to
// the native messaging host.
const char kParentWindowSwitchName[] = "parent-window";
#endif  // defined(OS_WIN)

base::FilePath GetHostManifestPathFromCommandLine(
    const std::string& native_host_name) {
  const std::string& value =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kNativeMessagingHosts);
  if (value.empty())
    return base::FilePath();

  std::vector<std::string> hosts;
  base::SplitString(value, ',', &hosts);
  for (size_t i = 0; i < hosts.size(); ++i) {
    std::vector<std::string> key_and_value;
    base::SplitString(hosts[i], '=', &key_and_value);
    if (key_and_value.size() != 2)
      continue;
    if (key_and_value[0] == native_host_name)
      return base::FilePath::FromUTF8Unsafe(key_and_value[1]);
  }

  return base::FilePath();
}


// Default implementation on NativeProcessLauncher interface.
class NativeProcessLauncherImpl : public NativeProcessLauncher {
 public:
  NativeProcessLauncherImpl(bool allow_user_level_hosts,
                            intptr_t native_window);
  virtual ~NativeProcessLauncherImpl();

  virtual void Launch(const GURL& origin,
                      const std::string& native_host_name,
                      LaunchedCallback callback) const OVERRIDE;

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core(bool allow_user_level_hosts, intptr_t native_window);
    void Launch(const GURL& origin,
                const std::string& native_host_name,
                LaunchedCallback callback);
    void Detach();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void DoLaunchOnThreadPool(const GURL& origin,
                              const std::string& native_host_name,
                              LaunchedCallback callback);
    void PostErrorResult(const LaunchedCallback& callback, LaunchResult error);
    void PostResult(const LaunchedCallback& callback,
                    base::ProcessHandle process_handle,
                    base::PlatformFile read_file,
                    base::PlatformFile write_file);
    void CallCallbackOnIOThread(LaunchedCallback callback,
                                LaunchResult result,
                                base::ProcessHandle process_handle,
                                base::PlatformFile read_file,
                                base::PlatformFile write_file);

    bool detached_;

    bool allow_user_level_hosts_;

    // Handle of the native window corresponding to the extension.
    intptr_t window_handle_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(NativeProcessLauncherImpl);
};

NativeProcessLauncherImpl::Core::Core(bool allow_user_level_hosts,
                                      intptr_t window_handle)
    : detached_(false),
      allow_user_level_hosts_(allow_user_level_hosts),
      window_handle_(window_handle) {
}

NativeProcessLauncherImpl::Core::~Core() {
  DCHECK(detached_);
}

void NativeProcessLauncherImpl::Core::Detach() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  detached_ = true;
}

void NativeProcessLauncherImpl::Core::Launch(
    const GURL& origin,
    const std::string& native_host_name,
    LaunchedCallback callback) {
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&Core::DoLaunchOnThreadPool, this,
                            origin, native_host_name, callback));
}

void NativeProcessLauncherImpl::Core::DoLaunchOnThreadPool(
    const GURL& origin,
    const std::string& native_host_name,
    LaunchedCallback callback) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  if (!NativeMessagingHostManifest::IsValidName(native_host_name)) {
    PostErrorResult(callback, RESULT_INVALID_NAME);
    return;
  }

  std::string error_message;
  scoped_ptr<NativeMessagingHostManifest> manifest;

  // First check if the manifest location is specified in the command line.
  base::FilePath manifest_path =
      GetHostManifestPathFromCommandLine(native_host_name);
  if (manifest_path.empty()) {
    manifest_path =
        FindManifest(native_host_name, allow_user_level_hosts_, &error_message);
  }

  if (manifest_path.empty()) {
    LOG(ERROR) << "Can't find manifest for native messaging host "
               << native_host_name;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  manifest = NativeMessagingHostManifest::Load(manifest_path, &error_message);

  if (!manifest) {
    LOG(ERROR) << "Failed to load manifest for native messaging host "
               << native_host_name << ": " << error_message;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  if (manifest->name() != native_host_name) {
    LOG(ERROR) << "Failed to load manifest for native messaging host "
               << native_host_name
               << ": Invalid name specified in the manifest.";
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
  }

  if (!manifest->allowed_origins().MatchesSecurityOrigin(origin)) {
    // Not an allowed origin.
    PostErrorResult(callback, RESULT_FORBIDDEN);
    return;
  }

  base::FilePath host_path = manifest->path();
  if (!host_path.IsAbsolute()) {
    // On Windows host path is allowed to be relative to the location of the
    // manifest file. On all other platforms the path must be absolute.
#if defined(OS_WIN)
    host_path = manifest_path.DirName().Append(host_path);
#else  // defined(OS_WIN)
    LOG(ERROR) << "Native messaging host path must be absolute for "
               << native_host_name;
    PostErrorResult(callback, RESULT_NOT_FOUND);
    return;
#endif  // !defined(OS_WIN)
  }

  CommandLine command_line(host_path);
  command_line.AppendArg(origin.spec());

  // Pass handle of the native view window to the native messaging host. This
  // way the host will be able to create properly focused UI windows.
#if defined(OS_WIN)
  command_line.AppendSwitchASCII(kParentWindowSwitchName,
                                 base::Int64ToString(window_handle_));
#endif  // !defined(OS_WIN)

  base::ProcessHandle process_handle;
  base::PlatformFile read_file;
  base::PlatformFile write_file;
  if (NativeProcessLauncher::LaunchNativeProcess(
          command_line, &process_handle, &read_file, &write_file)) {
    PostResult(callback, process_handle, read_file, write_file);
  } else {
    PostErrorResult(callback, RESULT_FAILED_TO_START);
  }
}

void NativeProcessLauncherImpl::Core::CallCallbackOnIOThread(
    LaunchedCallback callback,
    LaunchResult result,
    base::ProcessHandle process_handle,
    base::PlatformFile read_file,
    base::PlatformFile write_file) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (detached_) {
    if (read_file != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(read_file);
    if (write_file != base::kInvalidPlatformFileValue)
      base::ClosePlatformFile(write_file);
    return;
  }

  callback.Run(result, process_handle, read_file, write_file);
}

void NativeProcessLauncherImpl::Core::PostErrorResult(
    const LaunchedCallback& callback,
    LaunchResult error) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread, this,
                 callback, error, base::kNullProcessHandle,
                 base::kInvalidPlatformFileValue,
                 base::kInvalidPlatformFileValue));
}

void NativeProcessLauncherImpl::Core::PostResult(
    const LaunchedCallback& callback,
    base::ProcessHandle process_handle,
    base::PlatformFile read_file,
    base::PlatformFile write_file) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread, this,
                 callback, RESULT_SUCCESS, process_handle, read_file,
                 write_file));
}

NativeProcessLauncherImpl::NativeProcessLauncherImpl(
    bool allow_user_level_hosts,
    intptr_t window_handle)
    : core_(new Core(allow_user_level_hosts, window_handle)) {
}

NativeProcessLauncherImpl::~NativeProcessLauncherImpl() {
  core_->Detach();
}

void NativeProcessLauncherImpl::Launch(const GURL& origin,
                                       const std::string& native_host_name,
                                       LaunchedCallback callback) const {
  core_->Launch(origin, native_host_name, callback);
}

}  // namespace

// static
scoped_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault(
    bool allow_user_level_hosts,
    gfx::NativeView native_view) {
  intptr_t window_handle = 0;
#if defined(OS_WIN)
  window_handle = reinterpret_cast<intptr_t>(
      views::HWNDForNativeView(native_view));
#endif
  return scoped_ptr<NativeProcessLauncher>(
      new NativeProcessLauncherImpl(allow_user_level_hosts, window_handle));
}

}  // namespace extensions
