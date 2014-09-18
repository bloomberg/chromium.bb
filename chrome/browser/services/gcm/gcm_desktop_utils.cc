// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_desktop_utils.h"

#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_version_info.h"
#include "components/gcm_driver/gcm_client.h"
#include "components/gcm_driver/gcm_client_factory.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/gcm_driver/gcm_driver_desktop.h"
#include "content/public/browser/browser_thread.h"

namespace gcm {

namespace {

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
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return GCMClient::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return GCMClient::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return GCMClient::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return GCMClient::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return GCMClient::CHANNEL_STABLE;
    default:
      NOTREACHED();
      return GCMClient::CHANNEL_UNKNOWN;
  }
}

std::string GetVersion() {
  chrome::VersionInfo version_info;
  return version_info.Version();
}

GCMClient::ChromeBuildInfo GetChromeBuildInfo() {
  GCMClient::ChromeBuildInfo chrome_build_info;
  chrome_build_info.platform = GetPlatform();
  chrome_build_info.channel = GetChannel();
  chrome_build_info.version = GetVersion();
  return chrome_build_info;
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
