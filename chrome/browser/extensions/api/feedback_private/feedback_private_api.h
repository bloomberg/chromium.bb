// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include "chrome/browser/extensions/api/feedback_private/feedback_service.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/feedback_private.h"

namespace extensions {

class FeedbackService;

class FeedbackPrivateAPI : public ProfileKeyedAPI {
 public:
  explicit FeedbackPrivateAPI(Profile* profile);
  virtual ~FeedbackPrivateAPI();

  FeedbackService* GetService() const;
  void RequestFeedback(const std::string& description_template,
                       const std::string& category_tag,
                       const GURL& page_url);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<FeedbackPrivateAPI>* GetFactoryInstance();

 private:
  friend class ProfileKeyedAPIFactory<FeedbackPrivateAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "FeedbackPrivateAPI";
  }

  Profile* const profile_;
  FeedbackService* service_;
};

class FeedbackPrivateGetUserEmailFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getUserEmail",
                             FEEDBACKPRIVATE_GETUSEREMAIL);

 protected:
  virtual ~FeedbackPrivateGetUserEmailFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class FeedbackPrivateGetSystemInformationFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getSystemInformation",
                             FEEDBACKPRIVATE_GETSYSTEMINFORMATION);

 protected:
  virtual ~FeedbackPrivateGetSystemInformationFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnCompleted(const SystemInformationList& sys_info);
};

class FeedbackPrivateSendFeedbackFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.sendFeedback",
                             FEEDBACKPRIVATE_SENDFEEDBACK);

 protected:
  virtual ~FeedbackPrivateSendFeedbackFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  void OnCompleted(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
