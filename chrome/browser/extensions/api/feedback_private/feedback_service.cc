// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/screenshot_source.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

ScreenshotDataPtr ConvertStringToScreenshotPtr(scoped_ptr<std::string> image) {
  ScreenshotDataPtr screenshot(new ScreenshotData);
  std::copy(image->begin(), image->end(), screenshot->begin());
  return screenshot;
}

}

namespace extensions {

FeedbackService::FeedbackService() {
}

FeedbackService::~FeedbackService() {
}

void FeedbackService::SendFeedback(
    Profile* profile,
    scoped_refptr<FeedbackData> feedback_data,
    const SendFeedbackCallback& callback) {
  send_feedback_callback_ = callback;
  feedback_data_ = feedback_data;

  if (feedback_data_->attached_file_url().is_valid()) {
    attached_file_reader_ = new BlobReader(
        profile, feedback_data_->attached_file_url(),
        base::Bind(&FeedbackService::AttachedFileCallback,
                   GetWeakPtr()));
    attached_file_reader_->Start();
  }

  if (feedback_data_->screenshot_url().is_valid()) {
    attached_file_reader_ = new BlobReader(
        profile, feedback_data_->screenshot_url(),
        base::Bind(&FeedbackService::ScreenshotCallback,
                   GetWeakPtr()));
    screenshot_reader_->Start();
  }

  CompleteSendFeedback();
}

void FeedbackService::AttachedFileCallback(scoped_ptr<std::string> data) {
  if (!data.get())
    feedback_data_->set_attached_file_url(GURL());
  else
    feedback_data_->set_attached_filedata(data.Pass());

  CompleteSendFeedback();
}

void FeedbackService::ScreenshotCallback(scoped_ptr<std::string> data) {
  if (!data.get())
    feedback_data_->set_screenshot_url(GURL());
  else
    feedback_data_->set_image(ConvertStringToScreenshotPtr(data.Pass()));

  CompleteSendFeedback();
}

void FeedbackService::CompleteSendFeedback() {
  // If either the blob URL is invalid (we never needed to read it), or if the
  // data exists in the feedback object (the read is completed).
  bool attached_file_completed =
      !feedback_data_->attached_file_url().is_valid() ||
      feedback_data_->attached_filedata();
  bool screenshot_completed =
      !feedback_data_->screenshot_url().is_valid() ||
      !feedback_data_->image().get();

  if (screenshot_completed && attached_file_completed) {
    // Signal the feedback object that the data from the feedback page has been
    // filled - the object will manage sending of the actual report.
    feedback_data_->FeedbackPageDataComplete();
    // TODO(rkc): Change this once we have FeedbackData/Util refactored to
    // report the status of the report being sent.
    send_feedback_callback_.Run(true);
  }
}

}  // namespace extensions
