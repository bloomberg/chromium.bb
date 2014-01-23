// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_client_factory.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gcm/gcm_client_impl.h"
#include "net/url_request/url_request_context_getter.h"

namespace gcm {

namespace {

static bool g_gcm_client_io_task_posted = false;
static base::LazyInstance<GCMClientImpl>::Leaky g_gcm_client =
    LAZY_INSTANCE_INITIALIZER;
static GCMClientFactory::TestingFactoryFunction g_gcm_client_factory = NULL;
static GCMClient* g_gcm_client_override = NULL;

checkin_proto::ChromeBuildProto_Platform GetPlatform() {
#if defined(OS_WIN)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_MAC;
#elif defined(OS_CHROMEOS)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_CROS;
#elif defined(OS_LINUX)
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
#else
  // For all other platforms, return as LINUX.
  return checkin_proto::ChromeBuildProto_Platform_PLATFORM_LINUX;
#endif
}

std::string GetVersion() {
  chrome::VersionInfo version_info;
  return version_info.Version();
}

checkin_proto::ChromeBuildProto_Channel GetChannel() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_STABLE;
    default:
      NOTREACHED();
      return checkin_proto::ChromeBuildProto_Channel_CHANNEL_UNKNOWN;
  };
}

void BuildClientFromIO(const scoped_refptr<net::URLRequestContextGetter>&
    url_request_context_getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(g_gcm_client == NULL);

  checkin_proto::ChromeBuildProto chrome_build_proto;
  chrome_build_proto.set_platform(GetPlatform());
  chrome_build_proto.set_chrome_version(GetVersion());
  chrome_build_proto.set_channel(GetChannel());

  base::FilePath gcm_store_path;
  CHECK(PathService::Get(chrome::DIR_USER_DATA, &gcm_store_path));
  gcm_store_path = gcm_store_path.Append(chrome::kGCMStoreDirname);

  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  g_gcm_client.Pointer()->Initialize(chrome_build_proto,
                                     gcm_store_path,
                                     blocking_task_runner,
                                     url_request_context_getter);
}

}  // namespace

// static
void GCMClientFactory::BuildClientFromUI() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (g_gcm_client_factory) {
    if (!g_gcm_client_override)
      g_gcm_client_override = g_gcm_client_factory();
    return;
  }

  if (g_gcm_client_io_task_posted)
    return;
  g_gcm_client_io_task_posted = true;

  // system_url_request_context_getter can only be retrieved from
  // UI thread.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter(
      g_browser_process->io_thread()->system_url_request_context_getter());

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&BuildClientFromIO, url_request_context_getter));
}

// static
GCMClient* GCMClientFactory::GetClient() {
  // TODO(jianli): Make test code also get the client on IO thread.
  if (g_gcm_client_override)
    return g_gcm_client_override;

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(!(g_gcm_client == NULL));

  return g_gcm_client.Pointer();
}

// static
void GCMClientFactory::SetTestingFactory(TestingFactoryFunction factory) {
  g_gcm_client_factory = factory;
  BuildClientFromUI();
}

GCMClientFactory::GCMClientFactory() {
}

GCMClientFactory::~GCMClientFactory() {
}

}  // namespace gcm
