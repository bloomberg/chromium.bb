// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/feedback_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_private_api.h"
#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"

using feedback::FeedbackData;

namespace feedback_util {

namespace {

extensions::FeedbackService* GetFeedbackService(Profile* profile) {
  return extensions::FeedbackPrivateAPI::GetFactoryInstance()
      ->Get(profile)
      ->GetService();
}

void OnGetSystemInformation(
    Profile* profile,
    const std::string& description,
    const SendSysLogFeedbackCallback& callback,
    std::unique_ptr<system_logs::SystemLogsResponse> sys_info) {
  scoped_refptr<FeedbackData> feedback_data(new FeedbackData());

  feedback_data->set_context(profile);
  feedback_data->set_description(description);
  feedback_data->SetAndCompressSystemInfo(std::move(sys_info));

  GetFeedbackService(profile)->SendFeedback(feedback_data, callback);
}

}  // namespace

void SendSysLogFeedback(Profile* profile,
                        const std::string& description,
                        const SendSysLogFeedbackCallback& callback) {
  // |fetcher| deletes itself after calling its callback.
  system_logs::SystemLogsFetcher* fetcher =
      system_logs::BuildChromeSystemLogsFetcher();
  fetcher->Fetch(
      base::Bind(&OnGetSystemInformation, profile, description, callback));
}

}  // namespace feedback_util
