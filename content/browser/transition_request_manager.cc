// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/transition_request_manager.h"

#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
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
    std::vector<std::pair<std::string, std::string> >* attributes) {
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

void TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    std::vector<GURL>& entering_stylesheets,
    const GURL& resolve_address) {
  if (headers == NULL)
    return;

  std::string transition_stylesheet;
  std::vector<std::pair<std::string, std::string> > attributes;
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

bool TransitionRequestManager::HasPendingTransitionRequest(
    int process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(process_id, render_frame_id);
  return (pending_transition_frames_.find(key) !=
          pending_transition_frames_.end());
}

void TransitionRequestManager::SetHasPendingTransitionRequest(
    int process_id,
    int render_frame_id,
    bool has_pending) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::pair<int, int> key(process_id, render_frame_id);
  if (has_pending) {
    pending_transition_frames_.insert(key);
  } else {
    pending_transition_frames_.erase(key);
  }
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
