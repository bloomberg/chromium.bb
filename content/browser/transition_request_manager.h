// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

template <typename T>
struct DefaultSingletonTraits;

namespace net {
class HttpResponseHeaders;
}
class GURL;

namespace content {

// This struct passes data about an imminent transition between threads.
struct TransitionLayerData {
  TransitionLayerData();
  ~TransitionLayerData();

  std::string markup;
  std::string css_selector;
  scoped_refptr<net::HttpResponseHeaders> response_headers;
  GURL request_url;
};

// TransitionRequestManager is used to handle bookkeeping for transition
// requests and responses.
//
// TransitionRequestManager is a singleton and should only be accessed on the IO
// thread.
//
class TransitionRequestManager {
 public:
  // Returns the singleton instance.
  CONTENT_EXPORT static TransitionRequestManager* GetInstance();

  // Parses out any transition-entering-stylesheet link headers from the
  // response headers.
  CONTENT_EXPORT static void ParseTransitionStylesheetsFromHeaders(
      const scoped_refptr<net::HttpResponseHeaders>& headers,
      std::vector<GURL>& entering_stylesheets,
      const GURL& resolve_address);

  // Returns whether the RenderFrameHost specified by the given IDs currently
  // has any pending transition request data. If so, we will have to delay the
  // response until the embedder resumes the request.
  bool HasPendingTransitionRequest(int render_process_id,
                                   int render_frame_id,
                                   const GURL& request_url,
                                   TransitionLayerData* transition_data);

  // Adds pending request data for a transition navigation for the
  // RenderFrameHost specified by the given IDs.
  CONTENT_EXPORT void AddPendingTransitionRequestData(
      int render_process_id,
      int render_frame_id,
      const std::string& allowed_destination_host_pattern,
      const std::string& css_selector,
      const std::string& markup);

  void ClearPendingTransitionRequestData(int render_process_id,
                                         int render_frame_id);

 private:
  class TransitionRequestData {
   public:
    TransitionRequestData();
    ~TransitionRequestData();
    void AddEntry(const std::string& allowed_destination_host_pattern,
                  const std::string& selector,
                  const std::string& markup);
    bool FindEntry(const GURL& request_url,
                    TransitionLayerData* transition_data);

   private:
    struct AllowedEntry {
      // These strings could have originated from a compromised renderer,
      // and should not be trusted or assumed safe. They are only used within
      // a sandboxed iframe with scripts disabled.
      std::string allowed_destination_host_pattern;
      std::string css_selector;
      std::string markup;

      AllowedEntry(const std::string& allowed_destination_host_pattern,
                   const std::string& css_selector,
                   const std::string& markup) :
        allowed_destination_host_pattern(allowed_destination_host_pattern),
        css_selector(css_selector),
        markup(markup) {}
    };
    std::vector<AllowedEntry> allowed_entries_;
  };

  friend struct DefaultSingletonTraits<TransitionRequestManager>;
  typedef std::map<std::pair<int, int>, TransitionRequestData>
      RenderFrameRequestDataMap;

  TransitionRequestManager();
  ~TransitionRequestManager();

  // Map of (render_process_host_id, render_frame_id) pairs of all
  // RenderFrameHosts that have pending cross-site requests and their data.
  // Used to pass information to the CrossSiteResourceHandler without doing a
  // round-trip between IO->UI->IO threads.
  RenderFrameRequestDataMap pending_transition_frames_;

  DISALLOW_COPY_AND_ASSIGN(TransitionRequestManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRANSITION_REQUEST_MANAGER_H_
