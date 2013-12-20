// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__
#define CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__

#include "base/strings/string16.h"
#include "chrome/browser/history/history_types.h"
#include "url/gurl.h"

class SkBitmap;

/////////////////////////////////////////////////////////////////////////////
//
// PageUsageData
//
// A per domain usage data structure to compute and manage most visited
// pages.
//
// See History::QueryPageUsageSince()
//
/////////////////////////////////////////////////////////////////////////////
class PageUsageData {
 public:
  explicit PageUsageData(history::SegmentID id);

  virtual ~PageUsageData();

  // Return the url ID
  history::SegmentID GetID() const {
    return id_;
  }

  void SetURL(const GURL& url) {
    url_ = url;
  }

  const GURL& GetURL() const {
    return url_;
  }

  void SetTitle(const base::string16& s) {
    title_ = s;
  }

  const base::string16& GetTitle() const {
    return title_;
  }

  void SetScore(double v) {
    score_ = v;
  }

  double GetScore() const {
    return score_;
  }

  // Sort predicate to sort instances by score (high to low)
  static bool Predicate(const PageUsageData* dud1, const PageUsageData* dud2);

 private:
  history::SegmentID id_;
  GURL url_;
  base::string16 title_;

  double score_;
};

#endif  // CHROME_BROWSER_HISTORY_PAGE_USAGE_DATA_H__
