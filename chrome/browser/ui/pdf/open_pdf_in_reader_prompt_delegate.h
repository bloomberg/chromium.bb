// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_OPEN_PDF_IN_READER_PROMPT_DELEGATE_H_
#define CHROME_BROWSER_UI_PDF_OPEN_PDF_IN_READER_PROMPT_DELEGATE_H_

#include "base/strings/string16.h"

namespace content {
struct LoadCommittedDetails;
}

class OpenPDFInReaderPromptDelegate {
 public:
  virtual ~OpenPDFInReaderPromptDelegate() {}

  virtual base::string16 GetMessageText() const = 0;

  virtual base::string16 GetAcceptButtonText() const = 0;

  virtual base::string16 GetCancelButtonText() const = 0;

  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const = 0;

  virtual void Accept() = 0;

  virtual void Cancel() = 0;
};

#endif  // CHROME_BROWSER_UI_PDF_OPEN_PDF_IN_READER_PROMPT_DELEGATE_H_
