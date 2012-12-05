// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

namespace google_util {
namespace chromeos {

namespace {

// Path to file that stores the RLZ brand code on ChromeOS.
const char kRLZBrandFilePath[] =
    FILE_PATH_LITERAL("/opt/oem/etc/BRAND_CODE");

// Reads the brand code from file |kRLZBrandFilePath|.
std::string ReadBrandFromFile() {
  std::string brand;
  FilePath brand_file_path(kRLZBrandFilePath);
  if (!file_util::ReadFileToString(brand_file_path, &brand))
    LOG(WARNING) << "Brand code file missing: " << brand_file_path.value();
  TrimWhitespace(brand, TRIM_ALL, &brand);
  return brand;
}

// Sets the brand code to |brand|.
void SetBrand(const base::Closure& callback, const std::string& brand) {
  g_browser_process->local_state()->SetString(prefs::kRLZBrand, brand);
  callback.Run();
}

}  // namespace

std::string GetBrand() {
  DCHECK(
      !content::BrowserThread::IsWellKnownThread(content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(g_browser_process->local_state());
  return g_browser_process->local_state()->GetString(prefs::kRLZBrand);
}

void SetBrandFromFile(const base::Closure& callback) {
  base::PostTaskAndReplyWithResult(
      base::WorkerPool::GetTaskRunner(false /* task_is_slow */),
      FROM_HERE,
      base::Bind(&ReadBrandFromFile),
      base::Bind(&SetBrand, callback));
}

}  // namespace chromeos
}  // namespace google_util
