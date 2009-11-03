// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NAVIGATION_STATE_H_
#define CHROME_RENDERER_NAVIGATION_STATE_H_

#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/renderer/user_script_idle_scheduler.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/glue/alt_error_page_resource_fetcher.h"
#include "webkit/glue/password_form.h"

// The RenderView stores an instance of this class in the "extra data" of each
// WebDataSource (see RenderView::DidCreateDataSource).
class NavigationState : public WebKit::WebDataSource::ExtraData {
 public:
  static NavigationState* CreateBrowserInitiated(
      int32 pending_page_id,
      PageTransition::Type transition_type,
      base::Time request_time) {
    return new NavigationState(transition_type, request_time, false,
                               pending_page_id);
  }

  static NavigationState* CreateContentInitiated() {
    // We assume navigations initiated by content are link clicks.
    return new NavigationState(PageTransition::LINK, base::Time(), true, -1);
  }

  static NavigationState* FromDataSource(WebKit::WebDataSource* ds) {
    return static_cast<NavigationState*>(ds->extraData());
  }

  UserScriptIdleScheduler* user_script_idle_scheduler() {
    return user_script_idle_scheduler_.get();
  }
  void set_user_script_idle_scheduler(UserScriptIdleScheduler* scheduler) {
    user_script_idle_scheduler_.reset(scheduler);
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id() const { return pending_page_id_; }

  // Contains the transition type that the browser specified when it
  // initiated the load.
  PageTransition::Type transition_type() const { return transition_type_; }
  void set_transition_type(PageTransition::Type type) {
    transition_type_ = type;
  }

  // The time that this navigation was requested.
  const base::Time& request_time() const {
    return request_time_;
  }
  void set_request_time(const base::Time& value) {
    request_time_ = value;
  }

  // The time that the document load started.
  const base::Time& start_load_time() const {
    return start_load_time_;
  }
  void set_start_load_time(const base::Time& value) {
    start_load_time_ = value;
  }

  // The time that the document load was committed.
  const base::Time& commit_load_time() const {
    return commit_load_time_;
  }
  void set_commit_load_time(const base::Time& value) {
    commit_load_time_ = value;
  }

  // The time that the document finished loading.
  const base::Time& finish_document_load_time() const {
    return finish_document_load_time_;
  }
  void set_finish_document_load_time(const base::Time& value) {
    finish_document_load_time_ = value;
  }

  // The time that the document and all subresources finished loading.
  const base::Time& finish_load_time() const {
    return finish_load_time_;
  }
  void set_finish_load_time(const base::Time& value) {
    finish_load_time_ = value;
  }

  // The time that painting first happened after a new navigation.
  const base::Time& first_paint_time() const {
    return first_paint_time_;
  }
  void set_first_paint_time(const base::Time& value) {
    first_paint_time_ = value;
  }

  // The time that painting first happened after the document finished loading.
  const base::Time& first_paint_after_load_time() const {
    return first_paint_after_load_time_;
  }
  void set_first_paint_after_load_time(const base::Time& value) {
    first_paint_after_load_time_ = value;
  }

  // True iff the histograms for the associated frame have been dumped.
  bool load_histograms_recorded() const {
    return load_histograms_recorded_;
  }
  void set_load_histograms_recorded(bool value) {
    load_histograms_recorded_ = value;
  }

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed() const { return request_committed_; }
  void set_request_committed(bool value) { request_committed_ = value; }

  // True if this navigation was not initiated via WebFrame::LoadRequest.
  bool is_content_initiated() const { return is_content_initiated_; }

  const GURL& searchable_form_url() const { return searchable_form_url_; }
  void set_searchable_form_url(const GURL& url) { searchable_form_url_ = url; }
  const std::string& searchable_form_encoding() const {
    return searchable_form_encoding_;
  }
  void set_searchable_form_encoding(const std::string& encoding) {
    searchable_form_encoding_ = encoding;
  }

  webkit_glue::PasswordForm* password_form_data() const {
    return password_form_data_.get();
  }
  void set_password_form_data(webkit_glue::PasswordForm* data) {
    password_form_data_.reset(data);
  }

  webkit_glue::AltErrorPageResourceFetcher* alt_error_page_fetcher() const {
    return alt_error_page_fetcher_.get();
  }
  void set_alt_error_page_fetcher(webkit_glue::AltErrorPageResourceFetcher* f) {
    alt_error_page_fetcher_.reset(f);
  }

  const std::string& security_info() const {
    return security_info_;
  }
  void set_security_info(const std::string& security_info) {
    security_info_ = security_info;
  }

  bool postpone_loading_data() const {
    return postpone_loading_data_;
  }
  void set_postpone_loading_data(bool postpone_loading_data) {
    postpone_loading_data_ = postpone_loading_data;
  }

  void clear_postponed_data() {
    postponed_data_.clear();
  }
  void append_postponed_data(const char* data, size_t data_len) {
    postponed_data_.append(data, data_len);
  }
  const std::string& postponed_data() const {
    return postponed_data_;
  }

 private:
  NavigationState(PageTransition::Type transition_type,
                  const base::Time& request_time,
                  bool is_content_initiated,
                  int32 pending_page_id)
      : transition_type_(transition_type),
        request_time_(request_time),
        load_histograms_recorded_(false),
        request_committed_(false),
        is_content_initiated_(is_content_initiated),
        pending_page_id_(pending_page_id),
        postpone_loading_data_(false),
        user_script_idle_scheduler_(NULL) {
  }

  PageTransition::Type transition_type_;
  base::Time request_time_;
  base::Time start_load_time_;
  base::Time commit_load_time_;
  base::Time finish_document_load_time_;
  base::Time finish_load_time_;
  base::Time first_paint_time_;
  base::Time first_paint_after_load_time_;
  bool load_histograms_recorded_;
  bool request_committed_;
  bool is_content_initiated_;
  int32 pending_page_id_;
  GURL searchable_form_url_;
  std::string searchable_form_encoding_;
  scoped_ptr<webkit_glue::PasswordForm> password_form_data_;
  scoped_ptr<webkit_glue::AltErrorPageResourceFetcher> alt_error_page_fetcher_;
  std::string security_info_;
  bool postpone_loading_data_;
  std::string postponed_data_;
  scoped_ptr<UserScriptIdleScheduler> user_script_idle_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

#endif  // CHROME_RENDERER_NAVIGATION_STATE_H_
