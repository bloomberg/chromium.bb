// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SPELLCHECK_MESSAGE_FILTER_H_

#include "chrome/browser/browser_message_filter.h"

// A message filter implementation that receives
// the platform spell checker requests from SpellCheckProvider.
class SpellCheckMessageFilter : public BrowserMessageFilter {
 public:
  SpellCheckMessageFilter();
  ~SpellCheckMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

 private:
  void OnPlatformRequestTextCheck(int route_id,
                                  int identifier,
                                  int document_tag,
                                  const string16& text);
};

#endif  // CHROME_BROWSER_SPELLCHECK_MESSAGE_FILTER_H_
