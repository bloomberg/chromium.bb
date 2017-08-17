// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiling_host/profiling_process_host.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiling/constants.mojom.h"
#include "chrome/common/profiling/profiling_constants.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace profiling {

ProfilingProcessHost::ProfilingProcessHost() {
  Add(this);
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ProfilingProcessHost::~ProfilingProcessHost() {
  Remove(this);
}

void ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected(
    const content::ChildProcessData& data) {
  // Ignore newly launched child process if only profiling the browser.
  if (mode_ == Mode::kBrowser)
    return;

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(
            FROM_HERE,
            base::BindOnce(
                &ProfilingProcessHost::BrowserChildProcessLaunchedAndConnected,
                base::Unretained(this), data));
    return;
  }
  content::BrowserChildProcessHost* host =
      content::BrowserChildProcessHost::FromID(data.id);
  if (!host)
    return;

  // Tell the child process to start profiling.
  profiling::mojom::MemlogClientPtr memlog_client;
  profiling::mojom::MemlogClientRequest request =
      mojo::MakeRequest(&memlog_client);
  BindInterface(host->GetHost(), std::move(request));
  base::ProcessId pid = base::GetProcId(data.handle);
  SendPipeToProfilingService(std::move(memlog_client), pid);
}

void ProfilingProcessHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  // Ignore newly launched renderer if only profiling the browser.
  if (mode_ == Mode::kBrowser)
    return;

  if (type != content::NOTIFICATION_RENDERER_PROCESS_CREATED)
    return;

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(FROM_HERE, base::BindOnce(&ProfilingProcessHost::Observe,
                                             base::Unretained(this), type,
                                             source, details));
    return;
  }

  // Tell the child process to start profiling.
  content::RenderProcessHost* host =
      content::Source<content::RenderProcessHost>(source).ptr();
  profiling::mojom::MemlogClientPtr memlog_client;
  profiling::mojom::MemlogClientRequest request =
      mojo::MakeRequest(&memlog_client);
  content::BindInterface(host, std::move(request));
  base::ProcessId pid = base::GetProcId(host->GetHandle());

  SendPipeToProfilingService(std::move(memlog_client), pid);
}

void ProfilingProcessHost::SendPipeToProfilingService(
    profiling::mojom::MemlogClientPtr memlog_client,
    base::ProcessId pid) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  mojo::edk::PlatformChannelPair data_channel;

  memlog_->AddSender(
      pid,
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle),
      base::BindOnce(&ProfilingProcessHost::SendPipeToClientProcess,
                     base::Unretained(this), std::move(memlog_client),
                     mojo::WrapPlatformFile(
                         data_channel.PassClientHandle().release().handle)));
}

void ProfilingProcessHost::SendPipeToClientProcess(
    profiling::mojom::MemlogClientPtr memlog_client,
    mojo::ScopedHandle handle) {
  memlog_client->StartProfiling(std::move(handle));
}

// static
ProfilingProcessHost* ProfilingProcessHost::EnsureStarted(
    content::ServiceManagerConnection* connection,
    Mode mode) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ProfilingProcessHost* host = GetInstance();
  host->SetMode(mode);
  host->MakeConnector(connection);
  host->LaunchAsService();
  return host;
}

// static
ProfilingProcessHost* ProfilingProcessHost::GetInstance() {
  return base::Singleton<
      ProfilingProcessHost,
      base::LeakySingletonTraits<ProfilingProcessHost>>::get();
}

void ProfilingProcessHost::RequestProcessDump(base::ProcessId pid) {
  if (!connector_) {
    LOG(ERROR)
        << "Requesting process dump when profiling process hasn't started.";
    return;
  }
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&ProfilingProcessHost::GetOutputFileOnBlockingThread,
                     base::Unretained(this), pid));
}

void ProfilingProcessHost::MakeConnector(
    content::ServiceManagerConnection* connection) {
  connector_ = connection->GetConnector()->Clone();
}

void ProfilingProcessHost::LaunchAsService() {
  // May get called on different threads, we need to be on the IO thread to
  // work.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    content::BrowserThread::GetTaskRunnerForThread(content::BrowserThread::IO)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&ProfilingProcessHost::LaunchAsService,
                                  base::Unretained(this)));
    return;
  }

  // Ideally, we'd just call StartProfilingForClient, to interface with the
  // memlog client in the current [browser] process, but ChromeContentClient is
  // not correctly hooked up for the browser process. The MemlogClient there is
  // never bound. Instead, we use the *second* MemlogClient instance in the
  // process [a member variable of ProfilingProcessHost], which we also don't
  // bind, but instead directly call StartProfiling.
  connector_->BindInterface(mojom::kServiceName, &memlog_);

  mojo::edk::PlatformChannelPair data_channel;
  // Unretained is safe because this class is a leaky singleton that owns the
  // client object.
  memlog_->AddSender(
      base::Process::Current().Pid(),
      mojo::WrapPlatformFile(data_channel.PassServerHandle().release().handle),
      base::BindOnce(&MemlogClient::StartProfiling,
                     base::Unretained(&memlog_client_),
                     mojo::WrapPlatformFile(
                         data_channel.PassClientHandle().release().handle)));
}

void ProfilingProcessHost::GetOutputFileOnBlockingThread(base::ProcessId pid) {
  base::FilePath user_data_dir;
  PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath output_path = user_data_dir.AppendASCII("memlog_dump.json.gz");
  base::File file(output_path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ProfilingProcessHost::HandleDumpProcessOnIOThread,
                     base::Unretained(this), pid, std::move(file)));
}

void ProfilingProcessHost::HandleDumpProcessOnIOThread(base::ProcessId pid,
                                                       base::File file) {
  mojo::ScopedHandle handle = mojo::WrapPlatformFile(file.TakePlatformFile());
  memlog_->DumpProcess(pid, std::move(handle));
}

void ProfilingProcessHost::SetMode(Mode mode) {
  mode_ = mode;
}

}  // namespace profiling
