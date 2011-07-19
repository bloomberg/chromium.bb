// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_DELEGATE_H_
#define CHROME_BROWSER_INSTANT_INSTANT_DELEGATE_H_
#pragma once

#include "base/string16.h"
#include "chrome/common/instant_types.h"

class TabContentsWrapper;

namespace gfx {
class Rect;
}

// InstantController's delegate. Normally the Browser implements this. See
// InstantController for details.
class InstantDelegate {
 public:
  // Invoked when instant starts loading, but before the preview tab contents is
  // ready to be shown. This may be used to animate between the states.
  // This is followed by ShowInstant and/or HideInstant.
  virtual void PrepareForInstant() = 0;

  // Invoked when the instant TabContents should be shown.
  virtual void ShowInstant(TabContentsWrapper* preview_contents) = 0;

  // Invoked when the instant TabContents should be hidden. Instant still may be
  // active at the time this is invoked. Use |is_active()| to determine if
  // instant is still active.
  virtual void HideInstant() = 0;

  // Invoked when the user does something that should result in the preview
  // TabContents becoming the active TabContents. The delegate takes ownership
  // of the supplied TabContents.
  virtual void CommitInstant(TabContentsWrapper* preview_contents) = 0;

  // Invoked when the suggested text is to change to |text|.
  virtual void SetSuggestedText(const string16& text,
                                InstantCompleteBehavior behavior) = 0;

  // Returns the bounds instant will be placed at in screen coordinates.
  virtual gfx::Rect GetInstantBounds() = 0;

 protected:
  virtual ~InstantDelegate() {}
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_DELEGATE_H_
