// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_cycler/page_cycler.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/chrome_process_util.h"
#include "chrome/test/perf/perf_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

PageCycler::PageCycler(Browser* browser,
                       const base::FilePath& urls_file)
    : content::WebContentsObserver(
          browser->tab_strip_model()->GetActiveWebContents()),
      browser_(browser),
      urls_file_(urls_file),
      url_index_(0),
      aborted_(false) {
  BrowserList::AddObserver(this);
  AddRef();  // Balanced in Finish()/Abort() (only one should be called).
}

PageCycler::~PageCycler() {
}

bool PageCycler::IsLoadCallbackValid(const GURL& validated_url,
                                     bool is_main_frame) {
  // If |url_index_| is equal to zero, that means that this was called before
  // LoadNextURL() - this can happen at startup, loading the new tab page; or
  // if the user specified a bad url as the final url in the list. In these
  // cases, do not report success or failure, and load the next page.
  if (!url_index_) {
    LoadNextURL();
    return false;
  }
  return (is_main_frame &&
          validated_url.spec() != content::kUnreachableWebDataURL);
}

void PageCycler::DidFinishLoad(int64 frame_id,
                               const GURL& validated_url,
                               bool is_main_frame,
                               content::RenderViewHost* render_view_host) {
  if (IsLoadCallbackValid(validated_url, is_main_frame))
    LoadSucceeded();
}

void PageCycler::DidFailProvisionalLoad(
    int64 frame_id,
    bool is_main_frame,
    const GURL& validated_url,
    int error_code,
    const string16& error_description,
    content::RenderViewHost* render_view_host) {
  if (IsLoadCallbackValid(validated_url, is_main_frame))
    LoadFailed(validated_url, error_description);
}

void PageCycler::Run() {
  if (browser_)
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&PageCycler::ReadURLsOnBackgroundThread, this));
}

void PageCycler::ReadURLsOnBackgroundThread() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::string file_contents;
  std::vector<std::string> url_strings;

  CHECK(file_util::PathExists(urls_file_)) << urls_file_.value();
  file_util::ReadFileToString(urls_file_, &file_contents);
  base::SplitStringAlongWhitespace(file_contents, &url_strings);

  if (!url_strings.size()) {
#if defined(OS_POSIX)
    error_.append(ASCIIToUTF16("Page Cycler: No URLs in given file: " +
        urls_file_.value()));
#elif defined(OS_WIN)
    error_.append(ASCIIToUTF16("Page Cycler: No URLs in given file: "))
          .append(urls_file_.value());
#endif  // OS_WIN
  }

  for (std::vector<std::string>::const_iterator iter = url_strings.begin();
       iter != url_strings.end(); ++iter) {
    GURL gurl(*iter);
    if (!gurl.is_valid())
      error_.append(ASCIIToUTF16("Omitting invalid URL: " + *iter + ".\n"));
    // Since we don't count kUnreachableWebData as a valid load, we don't want
    // the user to specify this as one of the pages to visit.
    else if (*iter == content::kUnreachableWebDataURL) {
      error_.append(ASCIIToUTF16(
          "Chrome error pages are not allowed as urls. Omitting url: " +
          *iter + ".\n"));
    } else {
      urls_.push_back(gurl);
    }
  }

  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(&PageCycler::BeginCycle, this));
}

void PageCycler::BeginCycle() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  CHECK(browser_);
  // Upon launch, Chrome will automatically load the newtab page. This can
  // result in the browser being in a state of loading when PageCycler is ready
  // to start. Instead of interrupting the load, we wait for it to finish, and
  // will call LoadNextURL() from DidFinishLoad() or DidFailProvisionalLoad().
  if (browser_->tab_strip_model()->GetActiveWebContents()->IsLoading())
    return;
  LoadNextURL();
}

void PageCycler::LoadNextURL() {
  CHECK(browser_);
  if (url_index_ >= urls_.size()) {
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&PageCycler::PrepareResultsOnBackgroundThread, this));
    return;
  }
  if (url_index_) {
    timings_string_.append(", ");
    urls_string_.append(", ");
  }
  urls_string_.append(urls_[url_index_].spec());
  initial_time_ = base::TimeTicks::HighResNow();
  OpenURLParams params(urls_[url_index_],
                       Referrer(),
                       CURRENT_TAB,
                       content::PAGE_TRANSITION_TYPED,
                       false);
  ++url_index_;
  browser_->OpenURL(params);
}

void PageCycler::LoadSucceeded() {
  base::TimeDelta time_elapsed =
      (base::TimeTicks::HighResNow() - initial_time_) / 1000.0;
  timings_string_.append(base::Int64ToString(time_elapsed.ToInternalValue()));
  LoadNextURL();
}

void PageCycler::LoadFailed(const GURL& url,
                            const string16& error_description) {
  error_.append(ASCIIToUTF16("Failed to load the page at: " +
      url.spec() + ": ")).append(error_description).
      append(ASCIIToUTF16("\n"));
  base::TimeDelta time_elapsed =
      (base::TimeTicks::HighResNow() - initial_time_) / 1000.0;
  timings_string_.append(base::Int64ToString(time_elapsed.ToInternalValue()) +
      (" (failed)"));
  LoadNextURL();
}

void PageCycler::PrepareResultsOnBackgroundThread() {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  std::string output;
  if (!stats_file_.empty()) {
#if defined(OS_POSIX)
    base::ProcessId pid = base::GetParentProcessId(base::GetCurrentProcId());
#elif defined(OS_WIN)
    base::ProcessId pid = base::GetCurrentProcId();
#endif  // OS_WIN
    ChromeProcessList chrome_processes(GetRunningChromeProcesses(pid));
    output += perf_test::MemoryUsageInfoToString("", chrome_processes, pid);
    output += perf_test::IOPerfInfoToString("", chrome_processes, pid);
    output += perf_test::SystemCommitChargeToString("",
              base::GetSystemCommitCharge(), false);
    output.append("Pages: [" + urls_string_ + "]\n");
    output.append("*RESULT times: t_ref= [" + timings_string_ + "] ms\n");
  }
  WriteResultsOnBackgroundThread(output);
}

void PageCycler::WriteResultsOnBackgroundThread(const std::string& output) {
  CHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!output.empty()) {
    CHECK(!stats_file_.empty());
    if (file_util::PathExists(stats_file_)) {
      VLOG(1) << "PageCycler: Previous stats file found; appending.";
      file_util::AppendToFile(stats_file_, output.c_str(), output.size());
    } else {
      file_util::WriteFile(stats_file_, output.c_str(), output.size());
    }
  }
  if (!errors_file_.empty()) {
    if (!error_.empty()) {
      file_util::WriteFile(errors_file_, UTF16ToUTF8(error_).c_str(),
                           error_.size());
    } else if (file_util::PathExists(errors_file_)) {
      // If there is an old error file, delete it to avoid confusion.
      file_util::Delete(errors_file_, false);
    }
  }
  if (aborted_) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(&PageCycler::Abort, this));
  } else {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(&PageCycler::Finish, this));
  }
}

void PageCycler::Finish() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  BrowserList::RemoveObserver(this);
  browser_->OnWindowClosing();
  chrome::ExecuteCommand(browser_, IDC_EXIT);
  Release();  // Balanced in PageCycler constructor;
              // (only one of Finish/Abort should be called).
}

void PageCycler::Abort() {
  browser_ = NULL;
  BrowserList::RemoveObserver(this);
  Release();  // Balanced in PageCycler constructor;
              // (only one of Finish/Abort should be called).
}

void PageCycler::OnBrowserAdded(Browser* browser) {}

void PageCycler::OnBrowserRemoved(Browser* browser) {
  if (browser == browser_) {
    aborted_ = true;
    error_.append(ASCIIToUTF16(
        "Browser was closed before the run was completed."));
    DLOG(WARNING) <<
        "Page Cycler: browser was closed before the run was completed.";
    content::BrowserThread::PostBlockingPoolTask(
        FROM_HERE,
        base::Bind(&PageCycler::PrepareResultsOnBackgroundThread, this));
  }
}
