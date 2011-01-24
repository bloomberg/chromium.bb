// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_CONTENT_H_
#define CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_CONTENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/weak_ptr.h"
#include "chrome_frame/infobars/infobar_content.h"

class ReadyModeState;
class ReadyPromptWindow;
class UrlLauncher;

// Encapsulates the Ready Mode prompt inviting users to permanently activate
// Chrome Frame, temporarily disable Ready Mode, or permanently disable Ready
// Mode.
class ReadyPromptContent : public InfobarContent {
 public:
  // Takes ownership of the ReadyModeState and UrlLauncher instances, which
  // will be freed upon destruction of the ReadyPromptContent.
  ReadyPromptContent(ReadyModeState* ready_mode_state,
                     UrlLauncher* url_launcher);
  virtual ~ReadyPromptContent();

  // InfobarContent implementation
  virtual bool InstallInFrame(Frame* frame);
  virtual void SetDimensions(const RECT& dimensions);
  virtual size_t GetDesiredSize(size_t width, size_t height);

 private:
  base::WeakPtr<ReadyPromptWindow> window_;
  scoped_ptr<ReadyModeState> ready_mode_state_;
  scoped_ptr<UrlLauncher> url_launcher_;

  DISALLOW_COPY_AND_ASSIGN(ReadyPromptContent);
};  // class ReadyPromptContent

#endif  // CHROME_FRAME_READY_MODE_INTERNAL_READY_PROMPT_CONTENT_H_
