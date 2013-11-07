// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
#define CHROME_RENDERER_NET_NET_ERROR_HELPER_H_

#include <string>

#include "chrome/common/net/net_error_info.h"
#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/public/platform/WebURLError.h"

namespace base {
class DictionaryValue;
}

namespace blink {
class WebFrame;
}

// Listens for NetErrorInfo messages from the NetErrorTabHelper on the
// browser side and updates the error page with more details (currently, just
// DNS probe results) if/when available.
class NetErrorHelper : public content::RenderViewObserver {
 public:
  explicit NetErrorHelper(content::RenderView* render_view);
  virtual ~NetErrorHelper();

  // RenderViewObserver implementation.
  virtual void DidStartProvisionalLoad(blink::WebFrame* frame) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      blink::WebFrame* frame,
      const blink::WebURLError& error) OVERRIDE;
  virtual void DidCommitProvisionalLoad(
      blink::WebFrame* frame,
      bool is_new_navigation) OVERRIDE;
  virtual void DidFinishLoad(blink::WebFrame* frame) OVERRIDE;

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Examines |frame| and |error| to see if this is an error worthy of a DNS
  // probe.  If it is, initializes |error_strings| based on |error|,
  // |is_failed_post|, and |locale| with suitable strings and returns true.
  // If not, returns false, in which case the caller should look up error
  // strings directly using LocalizedError::GetNavigationErrorStrings.
  static bool GetErrorStringsForDnsProbe(
      blink::WebFrame* frame,
      const blink::WebURLError& error,
      bool is_failed_post,
      const std::string& locale,
      const std::string& accept_languages,
      base::DictionaryValue* error_strings);

 protected:
  // These methods handle tracking the actual state of the page; this allows
  // unit-testing of the state tracking without having to mock out WebFrames
  // and such.
  void OnStartLoad(bool is_main_frame, bool is_error_page);
  void OnFailLoad(bool is_main_frame, bool is_dns_error);
  void OnCommitLoad(bool is_main_frame);
  void OnFinishLoad(bool is_main_frame);

  void OnNetErrorInfo(int status);

  // |UpdateErrorPage| is virtual so it can be mocked out in the unittest.
  virtual void UpdateErrorPage();

  // The last DnsProbeStatus received from the browser.
  chrome_common_net::DnsProbeStatus last_probe_status_;

 private:
  blink::WebURLError GetUpdatedError() const;

  // Whether the last provisional load started was for an error page.
  bool last_start_was_error_page_;

  // Whether the last provisional load failure failed with a DNS error.
  bool last_fail_was_dns_error_;

  // Ideally, this would be simply "last_commit_was_dns_error_page_".
  //
  // Unfortunately, that breaks if two DNS errors occur in a row; after the
  // second failure, but before the second page commits, the helper can receive
  // probe results.  If all it knows is that the last commit was a DNS error
  // page, it will cheerfully forward the results for the second probe to the
  // first page.
  //
  // Thus, the semantics of this flag are a little weird.  It is set whenever
  // a DNS error page commits, and cleared whenever any other page commits,
  // but it is also cleared whenever a DNS error occurs, to prevent the race
  // described above.
  bool forwarding_probe_results_;

  // The last main frame error seen by the helper.
  blink::WebURLError last_error_;

  bool is_failed_post_;
};

#endif  // CHROME_RENDERER_NET_NET_ERROR_HELPER_H_
