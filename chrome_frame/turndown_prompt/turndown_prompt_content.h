// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_CONTENT_H_
#define CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_CONTENT_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome_frame/infobars/infobar_content.h"

class TurndownPromptWindow;
class UrlLauncher;

// Encapsulates the turndown prompt inviting users to learn about the turndown
// of Chrome Frame.
class TurndownPromptContent : public InfobarContent {
 public:
  // Takes ownership of the UrlLauncher instance, which will be freed upon
  // destruction of the TurndownPromptContent.
  TurndownPromptContent(UrlLauncher* url_launcher,
                        const base::Closure& uninstall_closure);
  virtual ~TurndownPromptContent();

  // InfobarContent implementation
  virtual bool InstallInFrame(Frame* frame);
  virtual void SetDimensions(const RECT& dimensions);
  virtual size_t GetDesiredSize(size_t width, size_t height);

 private:
  base::WeakPtr<TurndownPromptWindow> window_;
  scoped_ptr<UrlLauncher> url_launcher_;
  base::Closure uninstall_closure_;

  DISALLOW_COPY_AND_ASSIGN(TurndownPromptContent);
};  // class TurndownPromptContent

#endif  // CHROME_FRAME_TURNDOWN_PROMPT_TURNDOWN_PROMPT_CONTENT_H_
