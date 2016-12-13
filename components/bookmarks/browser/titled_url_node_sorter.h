// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_
#define COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_

#include <set>
#include <vector>

namespace bookmarks {

class TitledUrlNode;

class TitledUrlNodeSorter {
 public:
  using TitledUrlNodes = std::vector<const TitledUrlNode*>;
  using TitledUrlNodeSet = std::set<const TitledUrlNode*>;

  virtual ~TitledUrlNodeSorter() {};

  // Sorts |matches| in an implementation-specific way, placing the results in
  // |sorted_nodes|.
  virtual void SortMatches(const TitledUrlNodeSet& matches,
                           TitledUrlNodes* sorted_nodes) const = 0;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_TITLED_URL_NODE_SORTER_H_
