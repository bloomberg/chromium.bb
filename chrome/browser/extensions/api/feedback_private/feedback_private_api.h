// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/feedback_private.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "ui/gfx/rect.h"

namespace extensions {

extern char kFeedbackExtensionId[];

class FeedbackService;

using extensions::api::feedback_private::SystemInformation;

class FeedbackPrivateAPI : public BrowserContextKeyedAPI {
 public:
  explicit FeedbackPrivateAPI(content::BrowserContext* context);
  virtual ~FeedbackPrivateAPI();

  FeedbackService* GetService() const;
  void RequestFeedback(const std::string& description_template,
                       const std::string& category_tag,
                       const GURL& page_url);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>*
      GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<FeedbackPrivateAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "FeedbackPrivateAPI";
  }

  static const bool kServiceHasOwnInstanceInIncognito = true;

  content::BrowserContext* const browser_context_;
  FeedbackService* service_;
};

// Feedback strings.
class FeedbackPrivateGetStringsFunction : public ChromeSyncExtensionFunction {
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
  virtual bool RunSync() OVERRIDE;

 private:
  static base::Closure* test_callback_;
};

class FeedbackPrivateGetUserEmailFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getUserEmail",
                             FEEDBACKPRIVATE_GETUSEREMAIL);

 protected:
  virtual ~FeedbackPrivateGetUserEmailFunction() {}
  virtual bool RunSync() OVERRIDE;
};

class FeedbackPrivateGetSystemInformationFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.getSystemInformation",
                             FEEDBACKPRIVATE_GETSYSTEMINFORMATION);

 protected:
  virtual ~FeedbackPrivateGetSystemInformationFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnCompleted(
      const std::vector<linked_ptr<SystemInformation> >& sys_info);
};

class FeedbackPrivateSendFeedbackFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("feedbackPrivate.sendFeedback",
                             FEEDBACKPRIVATE_SENDFEEDBACK);

 protected:
  virtual ~FeedbackPrivateSendFeedbackFunction() {}
  virtual bool RunAsync() OVERRIDE;

 private:
  void OnCompleted(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FEEDBACK_PRIVATE_FEEDBACK_PRIVATE_API_H_
