// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"

#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_content_client.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/network_change_notifier.h"

using content::BrowserThread;
using feedback::FeedbackData;

namespace extensions {

FeedbackService::FeedbackService() {
}

FeedbackService::~FeedbackService() {
}

void FeedbackService::SendFeedback(
    Profile* profile,
    scoped_refptr<FeedbackData> feedback_data,
    const SendFeedbackCallback& callback) {
  feedback_data->set_locale(g_browser_process->GetApplicationLocale());
  feedback_data->set_user_agent(GetUserAgent());

  if (!feedback_data->attached_file_uuid().empty()) {
    // Self-deleting object.
    BlobReader* attached_file_reader =
        new BlobReader(profile, feedback_data->attached_file_uuid(),
                       base::Bind(&FeedbackService::AttachedFileCallback,
                                  AsWeakPtr(), feedback_data, callback));
    attached_file_reader->Start();
  }

  if (!feedback_data->screenshot_uuid().empty()) {
    // Self-deleting object.
    BlobReader* screenshot_reader =
        new BlobReader(profile, feedback_data->screenshot_uuid(),
                       base::Bind(&FeedbackService::ScreenshotCallback,
                                  AsWeakPtr(), feedback_data, callback));
    screenshot_reader->Start();
  }

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::GetSystemInformation(
    const system_logs::SysLogsFetcherCallback& callback) {
  system_logs::ScrubbedSystemLogsFetcher* fetcher =
      new system_logs::ScrubbedSystemLogsFetcher();
  fetcher->Fetch(callback);
}

void FeedbackService::AttachedFileCallback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback,
    std::unique_ptr<std::string> data,
    int64_t /* total_blob_length */) {
  feedback_data->set_attached_file_uuid(std::string());
  if (data)
    feedback_data->AttachAndCompressFileData(std::move(data));

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::ScreenshotCallback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback,
    std::unique_ptr<std::string> data,
    int64_t /* total_blob_length */) {
  feedback_data->set_screenshot_uuid(std::string());
  if (data)
    feedback_data->set_image(std::move(data));

  CompleteSendFeedback(feedback_data, callback);
}

void FeedbackService::CompleteSendFeedback(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    const SendFeedbackCallback& callback) {
  // A particular data collection is considered completed if,
  // a.) The blob URL is invalid - this will either happen because we never had
  //     a URL and never needed to read this data, or that the data read failed
  //     and we set it to invalid in the data read callback.
  // b.) The associated data object exists, meaning that the data has been read
  //     and the read callback has updated the associated data on the feedback
  //     object.
  const bool attached_file_completed =
      feedback_data->attached_file_uuid().empty();
  const bool screenshot_completed = feedback_data->screenshot_uuid().empty();

  if (screenshot_completed && attached_file_completed) {
    // Signal the feedback object that the data from the feedback page has been
    // filled - the object will manage sending of the actual report.
    feedback_data->OnFeedbackPageDataComplete();

    // Sending the feedback will be delayed if the user is offline.
    const bool result = !net::NetworkChangeNotifier::IsOffline();

    // TODO(rkc): Change this once we have FeedbackData/Util refactored to
    // report the status of the report being sent.
    callback.Run(result);
  }
}

}  // namespace extensions
