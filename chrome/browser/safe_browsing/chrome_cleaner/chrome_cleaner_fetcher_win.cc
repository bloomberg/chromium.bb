// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/common/channel_info.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/version_info/version_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

base::FilePath::StringType CleanerTempDirectoryPrefix() {
  base::FilePath::StringType common_prefix = FILE_PATH_LITERAL("ChromeCleaner");
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return common_prefix + FILE_PATH_LITERAL("_0_");
    case version_info::Channel::CANARY:
      return common_prefix + FILE_PATH_LITERAL("_1_");
    case version_info::Channel::DEV:
      return common_prefix + FILE_PATH_LITERAL("_2_");
    case version_info::Channel::BETA:
      return common_prefix + FILE_PATH_LITERAL("_3_");
    case version_info::Channel::STABLE:
      return common_prefix + FILE_PATH_LITERAL("_4_");
  }
  NOTREACHED();
  return common_prefix + FILE_PATH_LITERAL("_0_");
}

// Class that will attempt to download the Chrome Cleaner executable and call a
// given callback when done. Instances of ChromeCleanerFetcher own themselves
// and will self-delete if they encounter an error or when the network request
// has completed.
class ChromeCleanerFetcher : public net::URLFetcherDelegate {
 public:
  explicit ChromeCleanerFetcher(ChromeCleanerFetchedCallback fetched_callback);

 protected:
  ~ChromeCleanerFetcher() override;

 private:
  // Must be called on a sequence where IO is allowed.
  bool CreateTemporaryDirectory();
  // Will be called back on the same sequence as this object was created on.
  void OnTemporaryDirectoryCreated(bool success);
  void PostCallbackAndDeleteSelf(base::FilePath path,
                                 ChromeCleanerFetchStatus fetch_status);

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  ChromeCleanerFetchedCallback fetched_callback_;

  // The underlying URL fetcher. The instance is alive from construction through
  // OnURLFetchComplete.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // Used for file operations such as creating a new temporary directory.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;

  // We will take ownership of the scoped temp directory once we know that the
  // fetch has succeeded. Must be deleted on a sequence where IO is allowed.
  std::unique_ptr<base::ScopedTempDir, base::OnTaskRunnerDeleter>
      scoped_temp_dir_;
  base::FilePath temp_file_;

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerFetcher);
};

ChromeCleanerFetcher::ChromeCleanerFetcher(
    ChromeCleanerFetchedCallback fetched_callback)
    : fetched_callback_(std::move(fetched_callback)),
      url_fetcher_(net::URLFetcher::Create(0,
                                           GetSRTDownloadURL(),
                                           net::URLFetcher::GET,
                                           this)),
      blocking_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      scoped_temp_dir_(new base::ScopedTempDir(),
                       base::OnTaskRunnerDeleter(blocking_task_runner_)) {
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::Bind(&ChromeCleanerFetcher::CreateTemporaryDirectory,
                 base::Unretained(this)),
      base::Bind(&ChromeCleanerFetcher::OnTemporaryDirectoryCreated,
                 base::Unretained(this)));
}

ChromeCleanerFetcher::~ChromeCleanerFetcher() = default;

bool ChromeCleanerFetcher::CreateTemporaryDirectory() {
  base::FilePath temp_dir;
  return base::CreateNewTempDirectory(CleanerTempDirectoryPrefix(),
                                      &temp_dir) &&
         scoped_temp_dir_->Set(temp_dir);
}

void ChromeCleanerFetcher::OnTemporaryDirectoryCreated(bool success) {
  if (!success) {
    PostCallbackAndDeleteSelf(
        base::FilePath(),
        ChromeCleanerFetchStatus::kFailedToCreateTemporaryDirectory);
    return;
  }

  DCHECK(!scoped_temp_dir_->GetPath().empty());

  temp_file_ = scoped_temp_dir_->GetPath().Append(
      base::ASCIIToUTF16(base::GenerateGUID()) + L".tmp");

  data_use_measurement::DataUseUserData::AttachToFetcher(
      url_fetcher_.get(), data_use_measurement::DataUseUserData::SAFE_BROWSING);
  url_fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  url_fetcher_->SetMaxRetriesOn5xx(3);
  url_fetcher_->SaveResponseToFileAtPath(temp_file_, blocking_task_runner_);
  url_fetcher_->SetRequestContext(g_browser_process->system_request_context());
  url_fetcher_->Start();
}

void ChromeCleanerFetcher::PostCallbackAndDeleteSelf(
    base::FilePath path,
    ChromeCleanerFetchStatus fetch_status) {
  DCHECK(fetched_callback_);

  std::move(fetched_callback_).Run(std::move(path), fetch_status);

  // Since url_fetcher_ is passed a pointer to this object during construction,
  // explicitly destroy the url_fetcher_ to avoid potential destruction races.
  url_fetcher_.reset();

  // At this point, the url_fetcher_ is gone and this ChromeCleanerFetcher
  // instance is no longer needed.
  delete this;
}

void ChromeCleanerFetcher::OnURLFetchComplete(const net::URLFetcher* source) {
  // Take ownership of the fetcher in this scope (source == url_fetcher_).
  DCHECK_EQ(url_fetcher_.get(), source);
  DCHECK(!source->GetStatus().is_io_pending());
  DCHECK(fetched_callback_);

  if (source->GetResponseCode() == net::HTTP_NOT_FOUND) {
    PostCallbackAndDeleteSelf(base::FilePath(),
                              ChromeCleanerFetchStatus::kNotFoundOnServer);
    return;
  }

  base::FilePath download_path;
  if (!source->GetStatus().is_success() ||
      source->GetResponseCode() != net::HTTP_OK ||
      !source->GetResponseAsFilePath(/*take_ownership=*/true, &download_path)) {
    PostCallbackAndDeleteSelf(base::FilePath(),
                              ChromeCleanerFetchStatus::kOtherFailure);
    return;
  }

  DCHECK(!download_path.empty());
  DCHECK_EQ(temp_file_.value(), download_path.value());

  // Take ownership of the scoped temp directory so it is not deleted.
  scoped_temp_dir_->Take();

  PostCallbackAndDeleteSelf(std::move(download_path),
                            ChromeCleanerFetchStatus::kSuccess);
}

}  // namespace

void FetchChromeCleaner(ChromeCleanerFetchedCallback fetched_callback) {
  new ChromeCleanerFetcher(std::move(fetched_callback));
}

}  // namespace safe_browsing
