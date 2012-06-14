// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class TabContents;

// Objects implement this interface to get notified about changes in the
// TabContents and to provide necessary functionality.
//
// This is considered part of the TabContents "core" interface. If a piece of
// code interacts with TabContent, it is guaranteed that it will need to handle
// all of these callbacks.
class CoreTabHelperDelegate {
 public:
  virtual void SwapTabContents(TabContents* old_tc,
                               TabContents* new_tc);

  // Whether the specified TabContent can be reloaded.
  // Reloading can be disabled e. g. for the DevTools window.
  virtual bool CanReloadContents(TabContents* source) const;

  // Whether the specified TabContent can be saved.
  // Saving can be disabled e. g. for the DevTools window.
  virtual bool CanSaveContents(TabContents* source) const;

 protected:
  virtual ~CoreTabHelperDelegate();
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_DELEGATE_H_
