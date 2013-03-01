// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_split.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/api/messaging/native_messaging_host_manifest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace {

const char kNativeHostsDirectoryName[] = "native_hosts";

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
  NativeProcessLauncherImpl();
  virtual ~NativeProcessLauncherImpl();

  virtual void Launch(const GURL& origin,
                      const std::string& native_host_name,
                      LaunchedCallback callback) const OVERRIDE;

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core();
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
    void CallCallbackOnIOThread(LaunchedCallback callback,
                                bool result,
                                base::PlatformFile read_file,
                                base::PlatformFile write_file);

    bool detached_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  scoped_refptr<Core> core_;

  DISALLOW_COPY_AND_ASSIGN(NativeProcessLauncherImpl);
};

NativeProcessLauncherImpl::Core::Core()
    : detached_(false) {
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

  std::string error_message;
  scoped_ptr<NativeMessagingHostManifest> manifest;

  if (!NativeMessagingHostManifest::IsValidName(native_host_name)) {
    error_message = "Invalid native host name: " + native_host_name;
  } else {
    // First check if the manifest location is specified in the command line.
    base::FilePath path = GetHostManifestPathFromCommandLine(native_host_name);
    if (!path.empty()) {
      manifest = NativeMessagingHostManifest::Load(path, &error_message);
    } else {
      // Try loading the manifest from the default location.
      manifest = FindAndLoadManifest(native_host_name, &error_message);
    }

    if (manifest && manifest->name() != native_host_name) {
      error_message = "Name specified in the manifest does not match.";
      manifest.reset();
    }
  }

  if (!manifest) {
    // TODO(sergeyu): Report the error to the application.
    LOG(ERROR) << "Failed to load manifest for native messaging host "
               << native_host_name << ": " << error_message;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                   this, callback, false,
                   base::kInvalidPlatformFileValue,
                   base::kInvalidPlatformFileValue));
    return;
  }

  if (!manifest->allowed_origins().MatchesSecurityOrigin(origin)) {
    // Not an allowed origin.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                   this, callback, false,
                   base::kInvalidPlatformFileValue,
                   base::kInvalidPlatformFileValue));
    return;
  }

  base::PlatformFile read_file;
  base::PlatformFile write_file;
  bool result = NativeProcessLauncher::LaunchNativeProcess(
      manifest->path(), &read_file, &write_file);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&NativeProcessLauncherImpl::Core::CallCallbackOnIOThread,
                 this, callback, result, read_file, write_file));
}

void NativeProcessLauncherImpl::Core::CallCallbackOnIOThread(
    LaunchedCallback callback,
    bool result,
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

  callback.Run(result, read_file, write_file);
}

NativeProcessLauncherImpl::NativeProcessLauncherImpl()
  : core_(new Core()) {
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
scoped_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault() {
  return scoped_ptr<NativeProcessLauncher>(new NativeProcessLauncherImpl());
}

}  // namespace extensions
