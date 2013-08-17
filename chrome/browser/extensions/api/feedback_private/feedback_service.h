// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_

#include <vector>
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/feedback_private/blob_reader.h"
#include "chrome/browser/feedback/feedback_data.h"
#include "chrome/common/extensions/api/feedback_private.h"

class Profile;

using extensions::api::feedback_private::SystemInformation;

namespace extensions {

typedef std::vector<linked_ptr<SystemInformation> > SystemInformationList;

class FeedbackService {
 public:
  typedef base::Callback<void(bool)> SendFeedbackCallback;
  typedef base::Callback<void(const SystemInformationList& sys_info)>
      GetSystemInformationCallback;

  // Creates a platform-specific FeedbackService instance.
  static FeedbackService* CreateInstance();
  // Convenience method for populating a SystemInformationList structure
  // with a key/value pair.
  static void PopulateSystemInfo(SystemInformationList* sys_info_list,
                                 const std::string& key,
                                 const std::string& value);

  virtual ~FeedbackService();

  // Sends a feedback report.
  virtual void SendFeedback(Profile* profile,
                            scoped_refptr<FeedbackData> feedback_data,
                            const SendFeedbackCallback& callback);

  // Platform specific methods:
  // Get's the email address of the logged in user.
  virtual std::string GetUserEmail() = 0;

  // Start to gather system information.
  // The |callback| will be invoked once the query is completed.
  virtual void GetSystemInformation(
      const GetSystemInformationCallback& callback) = 0;

 protected:
  FeedbackService();

  // Used to get a weak ptr for a derived class instance.
  virtual base::WeakPtr<FeedbackService> GetWeakPtr() = 0;

  // Callbacks to receive blob data.
  void AttachedFileCallback(scoped_ptr<std::string> data);
  void ScreenshotCallback(scoped_ptr<std::string> data);

  // Checks if we have read all the blobs we need to; signals the feedback
  // data object once all the requisite data has been populated.
  void CompleteSendFeedback();

  GetSystemInformationCallback system_information_callback_;
  SendFeedbackCallback send_feedback_callback_;

  scoped_refptr<FeedbackData> feedback_data_;
  BlobReader* attached_file_reader_;
  BlobReader* screenshot_reader_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_SERVICE_H_
