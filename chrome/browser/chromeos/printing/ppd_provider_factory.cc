// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/printing/ppd_cache.h"
#include "chromeos/printing/ppd_provider.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/google_api_keys.h"
#include "net/url_request/url_request_context_getter.h"

namespace chromeos {
namespace printing {

scoped_refptr<PpdProvider> CreateProvider(Profile* profile) {
  base::FilePath ppd_cache_path =
      profile->GetPath().Append(FILE_PATH_LITERAL("PPDCache"));

  auto cache = PpdCache::Create(ppd_cache_path,
                                content::BrowserThread::GetTaskRunnerForThread(
                                    content::BrowserThread::CACHE)
                                    .get());

  return PpdProvider::Create(g_browser_process->GetApplicationLocale(),
                             g_browser_process->system_request_context(), cache,
                             content::BrowserThread::GetTaskRunnerForThread(
                                 content::BrowserThread::FILE)
                                 .get());
}

}  // namespace printing
}  // namespace chromeos
