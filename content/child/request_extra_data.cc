// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/request_extra_data.h"

using blink::WebString;

namespace content {

RequestExtraData::RequestExtraData() {
}

void RequestExtraData::set_visibility_state(
    blink::WebPageVisibilityState visibility_state) {
  visibility_state_ = visibility_state;
}

void RequestExtraData::set_render_frame_id(int render_frame_id) {
  render_frame_id_ = render_frame_id;
}

void RequestExtraData::set_is_main_frame(bool is_main_frame) {
  is_main_frame_ = is_main_frame;
}

void RequestExtraData::set_frame_origin(const GURL& frame_origin) {
  frame_origin_ = frame_origin;
}

void RequestExtraData::set_parent_is_main_frame(bool parent_is_main_frame) {
  parent_is_main_frame_ = parent_is_main_frame;
}

void RequestExtraData::set_parent_render_frame_id(int parent_render_frame_id) {
  parent_render_frame_id_ = parent_render_frame_id;
}

void RequestExtraData::set_allow_download(bool allow_download) {
  allow_download_ = allow_download;
}

void RequestExtraData::set_transition_type(PageTransition transition_type) {
  transition_type_ = transition_type;
}

void RequestExtraData::set_should_replace_current_entry(
    bool should_replace_current_entry) {
  should_replace_current_entry_ = should_replace_current_entry;
}

void RequestExtraData::set_transferred_request_child_id(
    int transferred_request_child_id) {
  transferred_request_child_id_ = transferred_request_child_id;
}

void RequestExtraData::set_transferred_request_request_id(
    int transferred_request_request_id) {
  transferred_request_request_id_ = transferred_request_request_id;
}

void RequestExtraData::set_service_worker_provider_id(
    int service_worker_provider_id) {
  service_worker_provider_id_ = service_worker_provider_id;
}

void RequestExtraData::set_custom_user_agent(
    const blink::WebString& custom_user_agent) {
  custom_user_agent_ = custom_user_agent;
}

void RequestExtraData::set_was_after_preconnect_request(
    bool was_after_preconnect_request) {
  was_after_preconnect_request_ = was_after_preconnect_request;
}

RequestExtraData::~RequestExtraData() {
}

}  // namespace content
