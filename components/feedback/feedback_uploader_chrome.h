// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

#include "components/feedback/feedback_uploader.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace feedback {

class FeedbackUploaderChrome : public FeedbackUploader,
                               public KeyedService {
 public:
  explicit FeedbackUploaderChrome(content::BrowserContext* context);

  virtual void DispatchReport(const std::string& data) OVERRIDE;

 private:
  // Browser context this uploader was created for.
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderChrome);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
