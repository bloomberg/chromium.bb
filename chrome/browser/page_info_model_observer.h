// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_MODEL_OBSERVER_H_
#define CHROME_BROWSER_PAGE_INFO_MODEL_OBSERVER_H_
#pragma once

// This interface should be implemented by classes interested in getting
// notifications from PageInfoModel.
class PageInfoModelObserver {
 public:
  virtual ~PageInfoModelObserver() {}

  virtual void OnPageInfoModelChanged() = 0;
};

#endif  // CHROME_BROWSER_PAGE_INFO_MODEL_OBSERVER_H_
