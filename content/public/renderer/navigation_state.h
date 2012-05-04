// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H_
#define CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H_
#pragma once

#include "content/public/common/page_transition_types.h"

namespace content {

// NavigationState is the portion of DocumentState that is affected by
// in-document navigation.
// TODO(simonjam): Move this to HistoryItem's ExtraData.
class NavigationState {
 public:
  virtual ~NavigationState();

  static NavigationState* CreateBrowserInitiated(
      int32 pending_page_id,
      int pending_history_list_offset,
      content::PageTransition transition_type) {
    return new NavigationState(transition_type, false, pending_page_id,
                               pending_history_list_offset);
  }

  static NavigationState* CreateContentInitiated() {
    return new NavigationState(content::PAGE_TRANSITION_LINK, true, -1, -1);
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id() const { return pending_page_id_; }

  // If pending_page_id() is not -1, then this contains the corresponding
  // offset of the page in the back/forward history list.
  int pending_history_list_offset() const {
    return pending_history_list_offset_;
  }

  // Contains the transition type that the browser specified when it
  // initiated the load.
  content::PageTransition transition_type() const { return transition_type_; }
  void set_transition_type(content::PageTransition type) {
    transition_type_ = type;
  }

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed() const { return request_committed_; }
  void set_request_committed(bool value) { request_committed_ = value; }

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  bool is_content_initiated() const { return is_content_initiated_; }

  // True iff the frame's navigation was within the same page.
  void set_was_within_same_page(bool value) { was_within_same_page_ = value; }
  bool was_within_same_page() const { return was_within_same_page_; }

  // transferred_request_child_id and transferred_request_request_id identify
  // a request that has been created before the navigation is being transferred
  // to a new renderer. This is used to recycle the old request once the new
  // renderer tries to pick up the navigation of the old one.
  void set_transferred_request_child_id(int value) {
    transferred_request_child_id_ = value;
  }
  int transferred_request_child_id() const {
    return transferred_request_child_id_;
  }
  void set_transferred_request_request_id(int value) {
    transferred_request_request_id_ = value;
  }
  int transferred_request_request_id() const {
    return transferred_request_request_id_;
  }
  void set_allow_download(bool value) {
    allow_download_ = value;
  }
  bool allow_download() const {
    return allow_download_;
  }

 private:
  NavigationState(content::PageTransition transition_type,
                  bool is_content_initiated,
                  int32 pending_page_id,
                  int pending_history_list_offset);

  content::PageTransition transition_type_;
  bool request_committed_;
  bool is_content_initiated_;
  int32 pending_page_id_;
  int pending_history_list_offset_;

  bool was_within_same_page_;
  int transferred_request_child_id_;
  int transferred_request_request_id_;
  bool allow_download_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

#endif  // CONTENT_PUBLIC_RENDERER_NAVIGATION_STATE_H_

}  // namespace content
