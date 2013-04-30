// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VALIDATION_MESSAGE_MESSAGE_FILTER_H_
#define CHROME_BROWSER_VALIDATION_MESSAGE_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"

namespace chrome {
class ValidationMessageBubble;
}

namespace gfx {
class Rect;
}

// A message filter implementation that receives validation message requests
// from ValidationMessageAgent.
class ValidationMessageMessageFilter : public content::BrowserMessageFilter {
 public:
  explicit ValidationMessageMessageFilter(int renderer_id);

  // content::BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OverrideThreadForMessage(
      const IPC::Message& message, content::BrowserThread::ID* thread) OVERRIDE;

 private:
  virtual ~ValidationMessageMessageFilter();
  void OnShowValidationMessage(int route_id,
                               const gfx::Rect& anchor_in_screen,
                               const string16& main_text,
                               const string16& sub_text);
  void OnHideValidationMessage();

  scoped_ptr<chrome::ValidationMessageBubble> validation_message_bubble_;
  int renderer_id_;

  DISALLOW_COPY_AND_ASSIGN(ValidationMessageMessageFilter);
};

#endif  // CHROME_BROWSER_VALIDATION_MESSAGE_MESSAGE_FILTER_H_
