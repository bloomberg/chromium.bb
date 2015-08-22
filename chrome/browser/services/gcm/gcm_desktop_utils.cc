// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_desktop_utils.h"

#include "base/command_line.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/services/gcm/gcm_desktop_utils.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/sync_util.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace gcm {

namespace {

const char kChannelStatusRelativePath[] = "/experimentstatus";

GCMClient::ChromePlatform GetPlatform() {
#if defined(OS_WIN)
  return GCMClient::PLATFORM_WIN;
#elif defined(OS_MACOSX)
  return GCMClient::PLATFORM_MAC;
#elif defined(OS_IOS)
  return GCMClient::PLATFORM_IOS;
#elif defined(OS_ANDROID)
  return GCMClient::PLATFORM_ANDROID;
#elif defined(OS_CHROMEOS)
  return GCMClient::PLATFORM_CROS;
#elif defined(OS_LINUX)
  return GCMClient::PLATFORM_LINUX;
#else
  // For all other platforms, return as LINUX.
  return GCMClient::PLATFORM_LINUX;
#endif
}

GCMClient::ChromeChannel GetChannel() {
  version_info::Channel channel = chrome::GetChannel();
  switch (channel) {
    case version_info::Channel::UNKNOWN:
      return GCMClient::CHANNEL_UNKNOWN;
    case version_info::Channel::CANARY:
      return GCMClient::CHANNEL_CANARY;
    case version_info::Channel::DEV:
      return GCMClient::CHANNEL_DEV;
    case version_info::Channel::BETA:
      return GCMClient::CHANNEL_BETA;
    case version_info::Channel::STABLE:
      return GCMClient::CHANNEL_STABLE;
    default:
      NOTREACHED();
      return GCMClient::CHANNEL_UNKNOWN;
  }
}

std::string GetVersion() {
  return version_info::GetVersionNumber();
}

GCMClient::ChromeBuildInfo GetChromeBuildInfo() {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.platform = GetPlatform();
  chrome_build_info.channel = GetChannel();
  chrome_build_info.version = GetVersion();
  return chrome_build_info;
}

std::string GetChannelStatusRequestUrl() {
  GURL sync_url(GetSyncServiceURL(*base::CommandLine::ForCurrentProcess()));
  return sync_url.spec() + kChannelStatusRelativePath;
}

std::string GetUserAgent() {
  return MakeDesktopUserAgentForSync();
}

}  // namespace

scoped_ptr<GCMDriver> CreateGCMDriverDesktop(
    scoped_ptr<GCMClientFactory> gcm_client_factory,
    PrefService* prefs,
    const base::FilePath& store_path,
    const scoped_refptr<net::URLRequestContextGetter>& request_context) {

  scoped_refptr<base::SequencedWorkerPool> worker_pool(
      content::BrowserThread::GetBlockingPool());
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner(
      worker_pool->GetSequencedTaskRunnerWithShutdownBehavior(
          worker_pool->GetSequenceToken(),
          base::SequencedWorkerPool::SKIP_ON_SHUTDOWN));

  return scoped_ptr<GCMDriver>(new GCMDriverDesktop(
      gcm_client_factory.Pass(),
      GetChromeBuildInfo(),
      GetChannelStatusRequestUrl(),
      GetUserAgent(),
      prefs,
      store_path,
      request_context,
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::UI),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO),
      blocking_task_runner));
}

}  // namespace gcm
