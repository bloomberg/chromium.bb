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

class TabStripModelExperimental;

// This class can represent either a single WebContents or a group of tabs.
class TabDataExperimental {
 public:
  enum class Type {
    // A single WebContents.
    kSingle,

    // A group of WebContentses.
    kGroup,

    // A group that also has a WebContents associated with it. This is the
    // "hub" of a sequence of navigations.
    kHubAndSpoke
  };

  TabDataExperimental();
  TabDataExperimental(TabDataExperimental&&) noexcept;

  // contents can be null only form kGroup type
  TabDataExperimental(TabDataExperimental* parent,
                      Type type,
                      content::WebContents* contents,
                      TabStripModelExperimental* model);
  ~TabDataExperimental();

  TabDataExperimental& operator=(TabDataExperimental&&);

  // Parent will be null for toplevel tabstrip items.
  const TabDataExperimental* parent() const { return parent_; }
  TabDataExperimental* parent() { return parent_; }

  Type type() const { return type_; }
  bool expanded() const { return expanded_; }

  content::WebContents* contents() { return contents_; }

  // Valid when type() == kSingle or kHubAndSpoke.
  const base::string16& GetTitle() const;

  // Returns true if this tab data itself is counted as a enumerable item when
  // going through the view.
  bool CountsAsViewIndex() const;

  // Valid when type() == kGroup or kHubAndSpoke;
  const std::vector<std::unique_ptr<TabDataExperimental>>& children() const {
    return children_;
  }
  std::vector<std::unique_ptr<TabDataExperimental>>& children() {
    return children_;
  }

 private:
  friend class TabStripModelExperimental;
  class ContentsWatcher;

  void ReplaceContents(content::WebContents* new_contents);

  TabDataExperimental* parent_ = nullptr;

  Type type_ = Type::kSingle;

  bool expanded_ = true;

  content::WebContents* contents_ = nullptr;
  std::unique_ptr<ContentsWatcher> contents_watcher_;

  std::vector<std::unique_ptr<TabDataExperimental>> children_;

  DISALLOW_COPY_AND_ASSIGN(TabDataExperimental);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_DATA_EXPERIMENTAL_H_
