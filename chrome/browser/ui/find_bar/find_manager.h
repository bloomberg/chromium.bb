// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FIND_BAR_FIND_MANAGER_H_
#define CHROME_BROWSER_UI_FIND_BAR_FIND_MANAGER_H_
#pragma once

#include "chrome/browser/tab_contents/tab_contents_observer.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"

class TabContentsWrapper;

// Per-tab find manager. Handles dealing with the life cycle of find sessions.
class FindManager : public TabContentsObserver {
 public:
  explicit FindManager(TabContentsWrapper* tab_contents);
  virtual ~FindManager();

  // Starts the Find operation by calling StartFinding on the Tab. This function
  // can be called from the outside as a result of hot-keys, so it uses the
  // last remembered search string as specified with set_find_string(). This
  // function does not block while a search is in progress. The controller will
  // receive the results through the notification mechanism. See Observe(...)
  // for details.
  void StartFinding(string16 search_string,
                    bool forward_direction,
                    bool case_sensitive);

  // Stops the current Find operation.
  void StopFinding(FindBarController::SelectionAction selection_action);

  // Accessors/Setters for find_ui_active_.
  bool find_ui_active() const { return find_ui_active_; }
  void set_find_ui_active(bool find_ui_active) {
      find_ui_active_ = find_ui_active;
  }

  // Setter for find_op_aborted_.
  void set_find_op_aborted(bool find_op_aborted) {
    find_op_aborted_ = find_op_aborted;
  }

  // Used _only_ by testing to get or set the current request ID.
  int current_find_request_id() { return current_find_request_id_; }
  void set_current_find_request_id(int current_find_request_id) {
    current_find_request_id_ = current_find_request_id;
  }

  // Accessor for find_text_. Used to determine if this TabContents has any
  // active searches.
  string16 find_text() const { return find_text_; }

  // Accessor for the previous search we issued.
  string16 previous_find_text() const { return previous_find_text_; }

  // Accessor for find_result_.
  const FindNotificationDetails& find_result() const {
    return last_search_result_;
  }

  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnFindReply(int request_id,
                   int number_of_matches,
                   const gfx::Rect& selection_rect,
                   int active_match_ordinal,
                   bool final_update);

 private:
  TabContentsWrapper* tab_contents_;

  // Each time a search request comes in we assign it an id before passing it
  // over the IPC so that when the results come in we can evaluate whether we
  // still care about the results of the search (in some cases we don't because
  // the user has issued a new search).
  static int find_request_id_counter_;

  // True if the Find UI is active for this Tab.
  bool find_ui_active_;

  // True if a Find operation was aborted. This can happen if the Find box is
  // closed or if the search term inside the Find box is erased while a search
  // is in progress. This can also be set if a page has been reloaded, and will
  // on FindNext result in a full Find operation so that the highlighting for
  // inactive matches can be repainted.
  bool find_op_aborted_;

  // This variable keeps track of what the most recent request id is.
  int current_find_request_id_;

  // The current string we are/just finished searching for. This is used to
  // figure out if this is a Find or a FindNext operation (FindNext should not
  // increase the request id).
  string16 find_text_;

  // The string we searched for before |find_text_|.
  string16 previous_find_text_;

  // Whether the last search was case sensitive or not.
  bool last_search_case_sensitive_;

  // The last find result. This object contains details about the number of
  // matches, the find selection rectangle, etc. The UI can access this
  // information to build its presentation.
  FindNotificationDetails last_search_result_;

  DISALLOW_COPY_AND_ASSIGN(FindManager);
};

#endif  // CHROME_BROWSER_UI_FIND_BAR_FIND_MANAGER_H_
