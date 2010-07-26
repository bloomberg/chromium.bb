// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECK_HOST_OBSERVER_H_
#define CHROME_BROWSER_SPELLCHECK_HOST_OBSERVER_H_
#pragma once

// Observer for the SpellCheckHost.
class SpellCheckHostObserver {
 public:
  // Invoked on the UI thread when SpellCheckHost is initialized.
  virtual void SpellCheckHostInitialized() = 0;

 protected:
  virtual ~SpellCheckHostObserver() {}
};

#endif  // CHROME_BROWSER_SPELLCHECK_HOST_OBSERVER_H_
