// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/transition_request_manager.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"

namespace {

// Enumerate all Link: headers with the specified relation in this
// response, and optionally returns the URL and any additional attributes of
// each one. See EnumerateHeaders for |iter| usage.
bool EnumerateLinkHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    void** iter,
    const std::string& rel,
    std::string* url,
    base::StringPairs* attributes) {
  std::string header_body;
  bool rel_matched = false;
  while (!rel_matched && headers->EnumerateHeader(iter, "link", &header_body)) {
    const std::string::const_iterator begin = header_body.begin();
    size_t url_start = header_body.find_first_of('<');
    size_t url_end = header_body.find_first_of('>');
    if (url_start == std::string::npos || url_end == std::string::npos ||
        url_start > url_end) {
      break;
    }

    if (attributes)
      attributes->clear();

    net::HttpUtil::NameValuePairsIterator param_iter(
        begin + url_end + 1, header_body.end(), ';');

    while (param_iter.GetNext()) {
      if (LowerCaseEqualsASCII(
              param_iter.name_begin(), param_iter.name_end(), "rel")) {
        if (LowerCaseEqualsASCII(param_iter.value_begin(),
                                 param_iter.value_end(),
                                 rel.c_str())) {
          if (url) {
            url->assign(begin + url_start + 1, begin + url_end);
          }
          rel_matched = true;
        } else {
          break;
        }
      } else if (attributes) {
        std::string attribute_name(param_iter.name_begin(),
                                   param_iter.name_end());
        std::string attribute_value(param_iter.value_begin(),
                                    param_iter.value_end());
        attributes->push_back(std::make_pair(attribute_name, attribute_value));
      }
    }
  }

  if (!rel_matched && attributes) {
    attributes->clear();
  }

  return rel_matched;
}

}  // namespace

namespace content {

TransitionLayerData::TransitionLayerData() {
}

TransitionLayerData::~TransitionLayerData() {
}

TransitionRequestManager::TransitionRequestData::AllowedEntry::AllowedEntry(
    const std::string& allowed_destination_host_pattern,
    const std::string& css_selector,
    const std::string& markup,
    const std::vector<TransitionElement>& elements)
    : allowed_destination_host_pattern(allowed_destination_host_pattern),
      css_selector(css_selector),
      markup(markup),
      elements(elements) {
}

TransitionRequestManager::TransitionRequestData::AllowedEntry::~AllowedEntry() {
}

void TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    std::vector<GURL>& entering_stylesheets,
    const GURL& resolve_address) {
  if (headers.get() == NULL)
    return;

  std::string transition_stylesheet;
  base::StringPairs attributes;
  void* header_iter = NULL;
  while (EnumerateLinkHeaders(headers,
                              &header_iter,
                              "transition-entering-stylesheet",
                              &transition_stylesheet,
                              &attributes)) {
    GURL stylesheet_url = resolve_address.Resolve(transition_stylesheet);
    if (stylesheet_url.is_valid())
      entering_stylesheets.push_back(stylesheet_url);
  }
}

TransitionRequestManager::TransitionRequestData::TransitionRequestData() {
}

TransitionRequestManager::TransitionRequestData::~TransitionRequestData() {
}

void TransitionRequestManager::TransitionRequestData::AddEntry(
    const std::string& allowed_destination_host_pattern,
    const std::string& css_selector,
    const std::string& markup,
    const std::vector<TransitionElement>& elements) {
  allowed_entries_.push_back(AllowedEntry(allowed_destination_host_pattern,
                                          css_selector,
                                          markup,
                                          elements));
}

bool TransitionRequestManager::TransitionRequestData::FindEntry(
    const GURL& request_url,
    TransitionLayerData* transition_data) {
  DCHECK(!allowed_entries_.empty());
  CHECK(transition_data);
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalWebPlatformFeatures) ||
        base::FieldTrialList::FindFullName("NavigationTransitions") ==
            "Enabled");

  for (const AllowedEntry& allowed_entry : allowed_entries_) {
    // Note: This is a small subset of the CSP source-list standard; once the
    // full CSP support is moved from the renderer to the browser, we should
    // use that instead.
    bool is_valid = (allowed_entry.allowed_destination_host_pattern == "*");
    if (!is_valid) {
      GURL allowed_host(allowed_entry.allowed_destination_host_pattern);
      if (allowed_host.is_valid() &&
          (allowed_host.GetOrigin() == request_url.GetOrigin())) {
        is_valid = true;
      }
    }

    if (is_valid) {
      transition_data->markup = allowed_entry.markup;
      transition_data->css_selector = allowed_entry.css_selector;
      transition_data->elements = allowed_entry.elements;
      return true;
    }
  }

  return false;
}

bool TransitionRequestManager::GetPendingTransitionRequest(
    int render_process_id,
    int render_frame_id,
    const GURL& request_url,
    TransitionLayerData* transition_data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(transition_data);
  std::pair<int, int> key(render_process_id, render_frame_id);
  RenderFrameRequestDataMap::iterator iter =
      pending_transition_frames_.find(key);
  return iter != pending_transition_frames_.end() &&
      iter->second.FindEntry(request_url, transition_data);
}

void TransitionRequestManager::AddPendingTransitionRequestData(
    int render_process_id,
    int render_frame_id,
    const std::string& allowed_destination_host_pattern,
    const std::string& css_selector,
    const std::string& markup,
    const std::vector<TransitionElement>& elements) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(render_process_id, render_frame_id);
  pending_transition_frames_[key].AddEntry(
      allowed_destination_host_pattern, css_selector, markup, elements);
}

void TransitionRequestManager::AddPendingTransitionRequestDataForTesting(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(render_process_id, render_frame_id);
  pending_transition_frames_[key].AddEntry(
      "*", /* allowed_destination_host_pattern */
      "", /* css_selector */
      "", /* markup */
      std::vector<TransitionElement>()); /* elements */
}

void TransitionRequestManager::ClearPendingTransitionRequestData(
    int render_process_id, int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  std::pair<int, int> key(render_process_id, render_frame_id);
  pending_transition_frames_.erase(key);
}

TransitionRequestManager::TransitionRequestManager() {
}

TransitionRequestManager::~TransitionRequestManager() {
}

// static
TransitionRequestManager* TransitionRequestManager::GetInstance() {
  return Singleton<TransitionRequestManager>::get();
}

}  // namespace content
