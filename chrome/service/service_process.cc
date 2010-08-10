// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/service_process.h"

#include <algorithm>

#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/service/cloud_print/cloud_print_proxy.h"
#include "chrome/service/service_ipc_server.h"
#include "net/base/network_change_notifier.h"

#if defined(ENABLE_REMOTING)
#include "remoting/base/encoder_zlib.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_config.h"

#if defined(OS_WIN)
#include "remoting/host/capturer_gdi.h"
#include "remoting/host/event_executor_win.h"
#elif defined(OS_LINUX)
#include "remoting/host/capturer_linux.h"
#include "remoting/host/event_executor_linux.h"
#elif defined(OS_MACOSX)
#include "remoting/host/capturer_mac.h"
#include "remoting/host/event_executor_mac.h"
#endif
#endif  // defined(ENABLED_REMOTING)

ServiceProcess* g_service_process = NULL;

ServiceProcess::ServiceProcess() : shutdown_event_(true, false) {
  DCHECK(!g_service_process);
  g_service_process = this;
}

bool ServiceProcess::Initialize() {
  network_change_notifier_.reset(net::NetworkChangeNotifier::Create());
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  io_thread_.reset(new base::Thread("ServiceProcess_IO"));
  file_thread_.reset(new base::Thread("ServiceProcess_File"));
  if (!io_thread_->StartWithOptions(options) ||
      !file_thread_->StartWithOptions(options)) {
    NOTREACHED();
    Teardown();
    return false;
  }
  FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  FilePath pref_path = user_data_dir.Append(chrome::kServiceStateFileName);
  service_prefs_.reset(new JsonPrefStore(pref_path,
                                         file_thread_->message_loop_proxy()));
  service_prefs_->ReadPrefs();
  // TODO(sanjeevr): We need to actually figure out the right way to determine
  // a channel name. The below is to facilitate testing only.
#if defined(OS_WIN)
  std::string channel_name = WideToUTF8(user_data_dir.value());
#elif defined(OS_POSIX)
  std::string channel_name = user_data_dir.value();
#endif  // defined(OS_WIN)

  std::replace(channel_name.begin(), channel_name.end(), '\\', '!');
  channel_name.append("_service_ipc");
  ipc_server_.reset(new ServiceIPCServer(channel_name));
  ipc_server_->Init();
  return true;
}

bool ServiceProcess::Teardown() {
  service_prefs_->WritePrefs();
  service_prefs_.reset();
  cloud_print_proxy_.reset();
  ipc_server_.reset();
  // Signal this event before shutting down the service process. That way all
  // background threads can cleanup.
  shutdown_event_.Signal();
  io_thread_.reset();
  file_thread_.reset();
  // The NetworkChangeNotifier must be destroyed after all other threads that
  // might use it have been shut down.
  network_change_notifier_.reset();
  return true;
}

CloudPrintProxy* ServiceProcess::GetCloudPrintProxy() {
  if (!cloud_print_proxy_.get()) {
    cloud_print_proxy_.reset(new CloudPrintProxy());
    cloud_print_proxy_->Initialize(service_prefs_.get());
  }
  return cloud_print_proxy_.get();
}

#if defined(ENABLE_REMOTING)
remoting::ChromotingHost* ServiceProcess::CreateChromotingHost(
    remoting::ChromotingHostContext* context,
    remoting::MutableHostConfig* config) {
  scoped_ptr<remoting::Capturer> capturer;
  scoped_ptr<remoting::Encoder> encoder;
  scoped_ptr<remoting::EventExecutor> executor;

  // Select the capturer and encoder from |config|.
#if defined(OS_WIN)
  capturer.reset(new remoting::CapturerGdi());
  executor.reset(new remoting::EventExecutorWin());
#elif defined(OS_LINUX)
  capturer.reset(new remoting::CapturerLinux());
  executor.reset(new remoting::EventExecutorLinux());
#elif defined(OS_MACOSX)
  capturer.reset(new remoting::CapturerMac());
  executor.reset(new remoting::EventExecutorMac());
#endif
  encoder.reset(new remoting::EncoderZlib());

  return new remoting::ChromotingHost(context, config, capturer.release(),
                                      encoder.release(), executor.release());
}
#endif

ServiceProcess::~ServiceProcess() {
  Teardown();
  g_service_process = NULL;
}
