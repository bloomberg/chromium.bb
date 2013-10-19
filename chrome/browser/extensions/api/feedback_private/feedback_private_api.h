// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/api/feedback_private.h"
#include "ui/gfx/rect.h"

namespace extensions {

extern char kFeedbackExtensionId[];

class FeedbackService;

using extensions::api::feedback_private::SystemInformation;

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

  static const bool kServiceRedirectedInIncognito = true;

  Profile* const profile_;
  FeedbackService* service_;
};

// Feedback strings.
class FeedbackPrivateGetStringsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getStrings",
                             FEEDBACKPRIVATE_GETSTRINGS)

  // Invoke this callback when this function is called - used for testing.
  static void set_test_callback(base::Closure* const callback) {
    test_callback_ = callback;
  }

 protected:
  virtual ~FeedbackPrivateGetStringsFunction() {}

  // SyncExtensionFunction overrides.
  virtual bool RunImpl() OVERRIDE;

 private:
  static base::Closure* test_callback_;
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
  void OnCompleted(
      const std::vector<linked_ptr<SystemInformation> >& sys_info);
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
