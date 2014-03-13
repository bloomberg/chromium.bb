// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/subtree_set.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {

SubtreeSet::Node::Node()
    : contained_as_subtree_root(false),
      number_of_subtrees_below(0) {
}

SubtreeSet::Node::Node(bool contained_as_subtree_root,
                       size_t number_of_subtrees_below)
    : contained_as_subtree_root(contained_as_subtree_root),
      number_of_subtrees_below(number_of_subtrees_below) {
}

SubtreeSet::SubtreeSet() {}
SubtreeSet::~SubtreeSet() {}

bool SubtreeSet::IsDisjointWith(const base::FilePath& subtree_root) const {
  DCHECK(fileapi::VirtualPath::IsAbsolute(subtree_root.value()));

  // Check if |subtree_root| contains any of subtrees in the container.
  if (ContainsKey(inclusive_ancestors_of_subtree_roots_, subtree_root.value()))
    return false;

  base::FilePath path = subtree_root;
  while (!fileapi::VirtualPath::IsRootPath(path)) {
    path = fileapi::VirtualPath::DirName(path);

    Subtrees::const_iterator found =
        inclusive_ancestors_of_subtree_roots_.find(path.value());
    if (found != inclusive_ancestors_of_subtree_roots_.end())
      return !found->second.contained_as_subtree_root;
  }

  return true;
}

bool SubtreeSet::Insert(const base::FilePath& subtree_root) {
  DCHECK(fileapi::VirtualPath::IsAbsolute(subtree_root.value()));

  if (!IsDisjointWith(subtree_root))
    return false;
  inclusive_ancestors_of_subtree_roots_[subtree_root.value()] = Node(true, 1);

  base::FilePath path = subtree_root;
  while (!fileapi::VirtualPath::IsRootPath(path)) {
    path = fileapi::VirtualPath::DirName(path);
    DCHECK(!inclusive_ancestors_of_subtree_roots_[path.value()]
                .contained_as_subtree_root);
    ++(inclusive_ancestors_of_subtree_roots_[path.value()]
           .number_of_subtrees_below);
  }

  return true;
}

bool SubtreeSet::Erase(const base::FilePath& subtree_root) {
  DCHECK(fileapi::VirtualPath::IsAbsolute(subtree_root.value()));

  {
    Subtrees::iterator found =
        inclusive_ancestors_of_subtree_roots_.find(subtree_root.value());
    if (found == inclusive_ancestors_of_subtree_roots_.end() ||
        !found->second.contained_as_subtree_root)
      return false;

    DCHECK_EQ(1u, found->second.number_of_subtrees_below);
    inclusive_ancestors_of_subtree_roots_.erase(found);
  }

  base::FilePath path = subtree_root;
  while (!fileapi::VirtualPath::IsRootPath(path)) {
    path = fileapi::VirtualPath::DirName(path);

    Subtrees::iterator found =
        inclusive_ancestors_of_subtree_roots_.find(path.value());
    if (found == inclusive_ancestors_of_subtree_roots_.end()) {
      NOTREACHED();
      continue;
    }

    DCHECK(!found->second.contained_as_subtree_root);
    if (!--(found->second.number_of_subtrees_below))
      inclusive_ancestors_of_subtree_roots_.erase(found);
  }

  return true;
}

size_t SubtreeSet::size() const {
  Subtrees::const_iterator found =
      inclusive_ancestors_of_subtree_roots_.find(fileapi::VirtualPath::kRoot);
  if (found == inclusive_ancestors_of_subtree_roots_.end())
    return 0;
  return found->second.number_of_subtrees_below;
}

}  // namespace sync_file_system
