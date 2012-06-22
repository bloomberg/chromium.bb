// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_H_
#pragma once

namespace chrome {
namespace search {

struct Mode;

// This class defines the observer interface for the |SearchModel|.
class SearchModelObserver {
 public:
  // Informs the observer that the mode has changed.
  virtual void ModeChanged(const Mode& mode) = 0;

 protected:
  virtual ~SearchModelObserver() {}
};

}  // namespace search
}  // namespace chrome

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_MODEL_OBSERVER_H_
