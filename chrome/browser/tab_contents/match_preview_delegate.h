// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_DELEGATE_H_
#define CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_DELEGATE_H_
#pragma once

#include "base/string16.h"

// MatchPreview's delegate. Normally the Browser implements this.  See
// MatchPreview for details.
class MatchPreviewDelegate {
 public:
  // Invoked when the preview TabContents should be shown.
  virtual void ShowMatchPreview() = 0;

  // Invoked when the preview TabContents should be hidden.
  virtual void HideMatchPreview() = 0;

  // Invoked when the user does something that should result in the preview
  // TabContents becoming the active TabContents.
  virtual void CommitMatchPreview() = 0;

  // Invoked when the suggested text is to change to |text|.
  virtual void SetSuggestedText(const string16& text) = 0;

 protected:
  virtual ~MatchPreviewDelegate() {}
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_MATCH_PREVIEW_DELEGATE_H_
