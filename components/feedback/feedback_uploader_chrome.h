// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
#define COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_

#include "base/macros.h"
#include "components/feedback/feedback_uploader.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace feedback {

class FeedbackUploaderChrome : public FeedbackUploader,
                               public KeyedService {
 public:
  FeedbackUploaderChrome(
      content::BrowserContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  // feedback::FeedbackUploader:
  void DispatchReport(scoped_refptr<FeedbackReport> report) override;

  // Browser context this uploader was created for.
  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderChrome);
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_FEEDBACK_UPLOADER_CHROME_H_
