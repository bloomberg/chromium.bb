// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_DATA_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_TABS_TAB_DATA_EXPERIMENTAL_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace content {
class WebContents;
}

class TabStripModel;

// This class can represent either a single WebContents or a group of tabs.
class TabDataExperimental {
 public:
  enum class Type { kSingle, kGroup };

  TabDataExperimental();
  TabDataExperimental(TabDataExperimental&&) noexcept;
  TabDataExperimental(content::WebContents* contents, TabStripModel* model);
  ~TabDataExperimental();

  TabDataExperimental& operator=(TabDataExperimental&&);

  Type type() const { return type_; }

  // Valid when type() == kSingle.
  const base::string16& GetTitle() const;

  // Valid when type() == kGroup;
  // bool first_is_hub() const { return first_is_hub_; }
  const std::vector<TabDataExperimental>& children() const { return children_; }

 private:
  friend class TabStripModelExperimental;
  class ContentsWatcher;

  void ReplaceContents(content::WebContents* new_contents);

  Type type_ = Type::kSingle;

  // Only valid when Type == kSingle. This is not exposed in the API so that
  // we can represent tabs that have no underlying WebContents in the future if
  // we want.
  content::WebContents* contents_ = nullptr;
  std::unique_ptr<ContentsWatcher> contents_watcher_;

  std::vector<TabDataExperimental> children_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DATA_EXPERIMENTAL_H_
