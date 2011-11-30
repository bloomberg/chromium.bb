// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class TabContentsWrapper;

// Objects implement this interface to get notified about changes in the
// TabContentsWrapper and to provide necessary functionality.
class TabContentsWrapperDelegate {
 public:
  virtual void SwapTabContents(TabContentsWrapper* old_tc,
                               TabContentsWrapper* new_tc) = 0;

 protected:
  virtual ~TabContentsWrapperDelegate();
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_TAB_CONTENTS_WRAPPER_DELEGATE_H_
