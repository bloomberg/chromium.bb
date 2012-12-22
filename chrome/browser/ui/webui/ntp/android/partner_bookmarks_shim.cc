// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/android/partner_bookmarks_shim.h"

#include "base/lazy_instance.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "content/public/browser/browser_thread.h"

using base::LazyInstance;
using content::BrowserThread;

namespace {

static LazyInstance<PartnerBookmarksShim> g_partner_bookmarks_shim_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

PartnerBookmarksShim::PartnerBookmarksShim()
    : bookmark_model_(NULL),
      attach_point_(NULL),
      loaded_(false),
      observers_(
          ObserverList<PartnerBookmarksShim::Observer>::NOTIFY_EXISTING_ONLY) {
}

PartnerBookmarksShim::~PartnerBookmarksShim() {
  FOR_EACH_OBSERVER(PartnerBookmarksShim::Observer, observers_,
                    ShimBeingDeleted(this));
}

PartnerBookmarksShim* PartnerBookmarksShim::GetInstance() {
  return g_partner_bookmarks_shim_instance.Pointer();
}

void PartnerBookmarksShim::Reset() {
  partner_bookmarks_root_.reset();
  bookmark_model_ = NULL;
  attach_point_ = NULL;
  loaded_ = false;
  observers_.Clear();
}

void PartnerBookmarksShim::AddObserver(
    PartnerBookmarksShim::Observer* observer) {
  observers_.AddObserver(observer);
}

void PartnerBookmarksShim::RemoveObserver(
    PartnerBookmarksShim::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PartnerBookmarksShim::IsPartnerBookmark(const BookmarkNode* node) {
  DCHECK(IsLoaded());
  if (!HasPartnerBookmarks())
    return false;
  const BookmarkNode* parent = node;
  while (parent != NULL) {
    if (parent == GetPartnerBookmarksRoot())
      return true;
    parent = parent->parent();
  }
  return false;
}

bool PartnerBookmarksShim::IsBookmarkEditable(const BookmarkNode* node) {
  return node && !IsPartnerBookmark(node) &&
      (node->type() == BookmarkNode::FOLDER ||
          node->type() == BookmarkNode::URL);
}

void PartnerBookmarksShim::AttachTo(
    BookmarkModel* bookmark_model,
    const BookmarkNode* attach_point) {
  if (bookmark_model_ == bookmark_model && attach_point_ == attach_point)
    return;
  if (attach_point && (attach_point->type() == BookmarkNode::FOLDER ||
      attach_point->type() == BookmarkNode::URL)) {
    DCHECK(false) << "Can not attach partner bookmarks to node of type: "
        << attach_point->type();
    return;
  }
  DCHECK(!bookmark_model_ || !bookmark_model);
  DCHECK(!attach_point_ || !attach_point);
  bookmark_model_ = bookmark_model;
  attach_point_ = attach_point;
}

const BookmarkNode* PartnerBookmarksShim::GetParentOf(
    const BookmarkNode* node) const {
  DCHECK(IsLoaded());
  DCHECK(node != NULL);
  if (HasPartnerBookmarks() && node == GetPartnerBookmarksRoot())
    return get_attach_point();
  return node->parent();
}

bool PartnerBookmarksShim::IsRootNode(const BookmarkNode* node) const {
  DCHECK(node != NULL);
  return node->is_root()
      && (!HasPartnerBookmarks() || node != GetPartnerBookmarksRoot());
}

const BookmarkNode* PartnerBookmarksShim::GetNodeByID(int64 id,
                                                      bool is_partner) const {
  DCHECK(IsLoaded());
  if (is_partner) {
    DCHECK(HasPartnerBookmarks());
    if (!HasPartnerBookmarks())
      return NULL;
    return GetNodeByID(GetPartnerBookmarksRoot(), id);
  }
  if (bookmark_model_)
    return bookmark_model_->GetNodeByID(id);
  return NULL;
}

const BookmarkNode* PartnerBookmarksShim::GetNodeByID(
    const BookmarkNode* parent, int64 id) const {
  if (parent->id() == id)
    return parent;
  for (int i = 0, child_count = parent->child_count(); i < child_count; ++i) {
    const BookmarkNode* result = GetNodeByID(parent->GetChild(i), id);
    if (result)
      return result;
  }
  return NULL;
}

bool PartnerBookmarksShim::IsLoaded() const {
  return loaded_;
}

bool PartnerBookmarksShim::HasPartnerBookmarks() const {
  DCHECK(IsLoaded());
  return partner_bookmarks_root_.get() != NULL;
}

const BookmarkNode* PartnerBookmarksShim::GetPartnerBookmarksRoot() const {
  DCHECK(HasPartnerBookmarks());
  return partner_bookmarks_root_.get();
}

void PartnerBookmarksShim::SetPartnerBookmarksRoot(BookmarkNode* root_node) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  partner_bookmarks_root_.reset(root_node);
  loaded_ = true;
  FOR_EACH_OBSERVER(PartnerBookmarksShim::Observer, observers_,
                    PartnerShimLoaded(this));
}
