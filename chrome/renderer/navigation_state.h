// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NAVIGATION_STATE_H_
#define CHROME_RENDERER_NAVIGATION_STATE_H_
#pragma once

#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "chrome/common/extensions/url_pattern.h"
#include "chrome/common/page_transition_types.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"

namespace webkit_glue {
struct PasswordForm;
class AltErrorPageResourceFetcher;
}

class UserScriptIdleScheduler;

// The RenderView stores an instance of this class in the "extra data" of each
// WebDataSource (see RenderView::DidCreateDataSource).
class NavigationState : public WebKit::WebDataSource::ExtraData {
 public:
  // The exact values of this enum are used in histograms, so new values must be
  // added to the end.
  enum LoadType {
    UNDEFINED_LOAD,            // Not yet initialized.
    RELOAD,                    // User pressed reload.
    HISTORY_LOAD,              // Back or forward.
    NORMAL_LOAD,               // User entered URL, or omnibox search.
    LINK_LOAD,                 // (deprecated) Included next 4 categories.
    LINK_LOAD_NORMAL,          // Commonly following of link.
    LINK_LOAD_RELOAD,          // JS/link directed reload.
    LINK_LOAD_CACHE_STALE_OK,  // back/forward or encoding change.
    LINK_LOAD_CACHE_ONLY,      // Allow stale data (avoid doing a re-post)
    PRERENDER_LOAD,            // Navigation started as the speculative
                               // prendering of a linked page.
    kLoadTypeMax               // Bounding value for this enum.
  };

  virtual ~NavigationState();

  static NavigationState* CreateBrowserInitiated(
      int32 pending_page_id,
      int pending_history_list_offset,
      PageTransition::Type transition_type,
      base::Time request_time) {
    return new NavigationState(transition_type, request_time, false,
                               pending_page_id,
                               pending_history_list_offset);
  }

  static NavigationState* CreateContentInitiated() {
    // We assume navigations initiated by content are link clicks.
    return new NavigationState(PageTransition::LINK, base::Time(), true, -1,
                               -1);
  }

  static NavigationState* FromDataSource(WebKit::WebDataSource* ds) {
    return static_cast<NavigationState*>(ds->extraData());
  }

  UserScriptIdleScheduler* user_script_idle_scheduler() {
    return user_script_idle_scheduler_.get();
  }
  void set_user_script_idle_scheduler(UserScriptIdleScheduler* scheduler);
  void swap_user_script_idle_scheduler(NavigationState* state);

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id() const { return pending_page_id_; }

  // If pending_page_id() is not -1, then this contains the corresponding
  // offset of the page in the back/forward history list.
  int pending_history_list_offset() const {
    return pending_history_list_offset_;
  }

  // Contains the transition type that the browser specified when it
  // initiated the load.
  PageTransition::Type transition_type() const { return transition_type_; }
  void set_transition_type(PageTransition::Type type) {
    transition_type_ = type;
  }

  // Record the nature of this load, for use when histogramming page load times.
  LoadType load_type() const { return load_type_; }
  void set_load_type(LoadType load_type) { load_type_ = load_type; }

  // The time that this navigation was requested.
  const base::Time& request_time() const {
    return request_time_;
  }
  void set_request_time(const base::Time& value) {
    DCHECK(start_load_time_.is_null());
    request_time_ = value;
  }

  // The time that the document load started.
  const base::Time& start_load_time() const {
    return start_load_time_;
  }
  void set_start_load_time(const base::Time& value) {
    // TODO(jar): This should not be set twice.
    // DCHECK(!start_load_time_.is_null());
    DCHECK(finish_document_load_time_.is_null());
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
    // TODO(jar): Some unittests break the following DCHECK, and don't have
    // DCHECK(!start_load_time_.is_null());
    DCHECK(!value.is_null());
    // TODO(jar): Double setting does happen, but probably shouldn't.
    // DCHECK(finish_document_load_time_.is_null());
    // TODO(jar): We should guarantee this order :-(.
    // DCHECK(finish_load_time_.is_null());
    finish_document_load_time_ = value;
  }

  // The time that the document and all subresources finished loading.
  const base::Time& finish_load_time() const { return finish_load_time_; }
  void set_finish_load_time(const base::Time& value) {
    DCHECK(!value.is_null());
    DCHECK(finish_load_time_.is_null());
    // The following is not already set in all cases :-(
    // DCHECK(!finish_document_load_time_.is_null());
    finish_load_time_ = value;
  }

  // The time that painting first happened after a new navigation.
  const base::Time& first_paint_time() const { return first_paint_time_; }
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

  // The time that a prerendered page was displayed.  Invalid for
  // non-prerendered pages.  Can be either before or after
  // |finish_document_load_time_|.
  const base::Time& prerendered_page_display_time() const;
  void set_prerendered_page_display_time(const base::Time& value);

  // True iff the histograms for the associated frame have been dumped.
  bool load_histograms_recorded() const { return load_histograms_recorded_; }
  void set_load_histograms_recorded(bool value) {
    load_histograms_recorded_ = value;
  }

  bool web_timing_histograms_recorded() const {
    return web_timing_histograms_recorded_;
  }
  void set_web_timing_histograms_recorded(bool value) {
    web_timing_histograms_recorded_ = value;
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
  void set_password_form_data(webkit_glue::PasswordForm* data);

  webkit_glue::AltErrorPageResourceFetcher* alt_error_page_fetcher() const {
    return alt_error_page_fetcher_.get();
  }
  void set_alt_error_page_fetcher(webkit_glue::AltErrorPageResourceFetcher* f);

  const std::string& security_info() const { return security_info_; }
  void set_security_info(const std::string& security_info) {
    security_info_ = security_info;
  }

  bool use_error_page() const { return use_error_page_; }
  void set_use_error_page(bool use_error_page) {
    use_error_page_ = use_error_page;
  }

  bool was_started_as_prerender() const;
  void set_was_started_as_prerender(bool was_started_as_prerender);

  int http_status_code() const { return http_status_code_; }
  void set_http_status_code(int http_status_code) {
    http_status_code_ = http_status_code;
  }

  // Sets the cache policy. The cache policy is only used if explicitly set and
  // by default is not set. You can mark a NavigationState as not having a cache
  // state by way of clear_cache_policy_override.
  void set_cache_policy_override(
      WebKit::WebURLRequest::CachePolicy cache_policy) {
    cache_policy_override_ = cache_policy;
    cache_policy_override_set_ = true;
  }
  WebKit::WebURLRequest::CachePolicy cache_policy_override() const {
    return cache_policy_override_;
  }
  void clear_cache_policy_override() {
    cache_policy_override_set_ = false;
    cache_policy_override_ = WebKit::WebURLRequest::UseProtocolCachePolicy;
  }
  bool is_cache_policy_override_set() const {
    return cache_policy_override_set_;
  }

  // Indicator if SPDY was used as part of this page load.
  void set_was_fetched_via_spdy(bool value) { was_fetched_via_spdy_ = value; }
  bool was_fetched_via_spdy() const { return was_fetched_via_spdy_; }

  void set_was_npn_negotiated(bool value) { was_npn_negotiated_ = value; }
  bool was_npn_negotiated() const { return was_npn_negotiated_; }

  void set_was_alternate_protocol_available(bool value) {
    was_alternate_protocol_available_ = value;
  }
  bool was_alternate_protocol_available() const {
    return was_alternate_protocol_available_;
  }

  void set_was_fetched_via_proxy(bool value) {
    was_fetched_via_proxy_ = value;
  }
  bool was_fetched_via_proxy() const { return was_fetched_via_proxy_; }

  // Whether the frame text contents was translated to a different language.
  void set_was_translated(bool value) { was_translated_ = value; }
  bool was_translated() const { return was_translated_; }

  // True iff the frame's navigation was within the same page.
  void set_was_within_same_page(bool value) { was_within_same_page_ = value; }
  bool was_within_same_page() const { return was_within_same_page_; }

  void set_was_prefetcher(bool value) { was_prefetcher_ = value; }
  bool was_prefetcher() const { return was_prefetcher_; }

  void set_was_referred_by_prefetcher(bool value) {
    was_referred_by_prefetcher_ = value;
  }
  bool was_referred_by_prefetcher() const {
    return was_referred_by_prefetcher_;
  }

 private:
  NavigationState(PageTransition::Type transition_type,
                  const base::Time& request_time,
                  bool is_content_initiated,
                  int32 pending_page_id,
                  int pending_history_list_offset);

  PageTransition::Type transition_type_;
  LoadType load_type_;
  base::Time request_time_;
  base::Time start_load_time_;
  base::Time commit_load_time_;
  base::Time finish_document_load_time_;
  base::Time finish_load_time_;
  base::Time first_paint_time_;
  base::Time first_paint_after_load_time_;
  base::Time prerendered_page_display_time_;
  bool load_histograms_recorded_;
  bool web_timing_histograms_recorded_;
  bool request_committed_;
  bool is_content_initiated_;
  int32 pending_page_id_;
  int pending_history_list_offset_;
  GURL searchable_form_url_;
  std::string searchable_form_encoding_;
  scoped_ptr<webkit_glue::PasswordForm> password_form_data_;
  scoped_ptr<webkit_glue::AltErrorPageResourceFetcher> alt_error_page_fetcher_;
  std::string security_info_;

  // True if we should use an error page, if the http status code alos indicates
  // an error.
  bool use_error_page_;

  // True if a page load started as a prerender.  Preserved across redirects.
  bool was_started_as_prerender_;

  bool cache_policy_override_set_;
  WebKit::WebURLRequest::CachePolicy cache_policy_override_;

  scoped_ptr<UserScriptIdleScheduler> user_script_idle_scheduler_;
  int http_status_code_;

  bool was_fetched_via_spdy_;
  bool was_npn_negotiated_;
  bool was_alternate_protocol_available_;
  bool was_fetched_via_proxy_;
  bool was_translated_;
  bool was_within_same_page_;

  // A prefetcher is a page that contains link rel=prefetch elements.
  bool was_prefetcher_;
  bool was_referred_by_prefetcher_;

  DISALLOW_COPY_AND_ASSIGN(NavigationState);
};

#endif  // CHROME_RENDERER_NAVIGATION_STATE_H_
