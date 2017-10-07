// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_H_
#define CHROME_BROWSER_UI_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Defines the messages shown to the user when the browser intervenes to block
// a framebust attempt, and provides a way to perform the blocked redirection
// if the user decides to do so.
class FramebustBlockMessageDelegate {
 public:
  FramebustBlockMessageDelegate(content::WebContents* web_contents,
                                const GURL& blocked_url,
                                base::OnceClosure click_closure);
  virtual ~FramebustBlockMessageDelegate();

  int GetIconId() const;
  base::string16 GetLongMessage() const;
  base::string16 GetShortMessage() const;
  const GURL& GetBlockedUrl() const;
  void OnLinkClicked();

 private:
  // Closure to be called when the link is clicked.
  base::OnceClosure click_closure_;

  // WebContents associated with the frame that was targeted by the framebust.
  // Will be used to continue the navigation to the blocked URL.
  content::WebContents* web_contents_;

  // The URL that was the redirection target in the blocked framebust attempt.
  const GURL blocked_url_;

  DISALLOW_COPY_AND_ASSIGN(FramebustBlockMessageDelegate);
};

#endif  // CHROME_BROWSER_UI_INTERVENTIONS_FRAMEBUST_BLOCK_MESSAGE_DELEGATE_H_
