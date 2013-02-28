// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_process_launcher.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_paths.h"

namespace extensions {

namespace {

const char kNativeHostsDirectoryName[] = "native_hosts";

// Default implementation on NativeProcessLauncher interface.
class NativeProcessLauncherImpl : public NativeProcessLauncher {
 public:
  NativeProcessLauncherImpl();
  virtual ~NativeProcessLauncherImpl();

  virtual void Launch(const std::string& native_host_name,
                      LaunchedCallback callback) const OVERRIDE;

 private:
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core();
    void Launch(const std::string& native_host_name,
                LaunchedCallback callback);
    void Detach();

   private:
    friend class base::RefCountedThreadSafe<Core>;
    virtual ~Core();

    void DoLaunchOnThreadPool(const std::string& native_host_name,
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
    const std::string& native_host_name,
    LaunchedCallback callback) {
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE, base::Bind(&Core::DoLaunchOnThreadPool, this,
                            native_host_name, callback));
}

void NativeProcessLauncherImpl::Core::DoLaunchOnThreadPool(
    const std::string& native_host_name,
    LaunchedCallback callback) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  base::FilePath native_host_program;
  base::FilePath native_host_registry;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &native_host_registry));
  native_host_registry =
      native_host_registry.AppendASCII(kNativeHostsDirectoryName);
  native_host_program = native_host_registry.AppendASCII(native_host_name);

  // Make sure that the client is not trying to invoke something outside of the
  // proper directory. Eg. '../../dangerous_something.exe'.
  if (!file_util::ContainsPath(native_host_registry, native_host_program)) {
    LOG(ERROR) << "Could not find native host: " << native_host_name;
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
      native_host_program, &read_file, &write_file);

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

void NativeProcessLauncherImpl::Launch(const std::string& native_host_name,
                                       LaunchedCallback callback) const {
  core_->Launch(native_host_name, callback);
}

}  // namespace

// static
scoped_ptr<NativeProcessLauncher> NativeProcessLauncher::CreateDefault() {
  return scoped_ptr<NativeProcessLauncher>(new NativeProcessLauncherImpl());
}

}  // namespace extensions
