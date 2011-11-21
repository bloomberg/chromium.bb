// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_SPELLING_BUBBLE_MODEL_H_
#define CHROME_BROWSER_TAB_CONTENTS_SPELLING_BUBBLE_MODEL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/ui/confirm_bubble_model.h"

class Profile;

// A class that implements a bubble menu shown when we confirm a user allows
// integrating the spelling service of Google to Chrome.
class SpellingBubbleModel : public ConfirmBubbleModel {
 public:
  explicit SpellingBubbleModel(Profile* profile);
  virtual ~SpellingBubbleModel();

  // ConfirmBubbleModel implementation.
  virtual string16 GetTitle() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual string16 GetButtonLabel(BubbleButton button) const OVERRIDE;
  virtual void Accept() OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual void LinkClicked() OVERRIDE;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_SPELLING_BUBBLE_MODEL_H_
