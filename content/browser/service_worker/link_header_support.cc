// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/link_header_support.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"

namespace content {

namespace {

// A variation of base::StringTokenizer and net::HttpUtil::ValuesIterator.
// Takes the parsing of StringTokenizer and adds support for quoted strings that
// are quoted by matching <> (and does not support escaping in those strings).
// Also has the behavior of ValuesIterator where it strips whitespace from all
// values and only outputs non-empty values.
// Only supports ',' as separator and supports '' "" and <> as quote chars.
// TODO(mek): Figure out if there is a way to share this with the parsing code
// in blink::LinkHeader.
class ValueTokenizer {
 public:
  ValueTokenizer(std::string::const_iterator begin,
                 std::string::const_iterator end)
      : token_begin_(begin), token_end_(begin), end_(end) {}

  std::string::const_iterator token_begin() const { return token_begin_; }
  std::string::const_iterator token_end() const { return token_end_; }

  bool GetNext() {
    while (GetNextInternal()) {
      net::HttpUtil::TrimLWS(&token_begin_, &token_end_);

      // Only return non-empty values.
      if (token_begin_ != token_end_)
        return true;
    }
    return false;
  }

 private:
  // Updates token_begin_ and token_end_ to point to the (possibly empty) next
  // token. Returns false if end-of-string was reached first.
  bool GetNextInternal() {
    // First time this is called token_end_ points to the first character in the
    // input. Every other time token_end_ points to the delimiter at the end of
    // the last returned token (which could be the end of the string).

    // End of string, return false.
    if (token_end_ == end_)
      return false;

    // Skip past the delimiter.
    if (*token_end_ == ',')
      ++token_end_;

    // Make token_begin_ point to the beginning of the next token, and search
    // for the end of the token in token_end_.
    token_begin_ = token_end_;

    // Set to true if we're currently inside a quoted string.
    bool in_quote = false;
    // Set to true if we're currently inside a quoted string, and have just
    // encountered an escape character. In this case a closing quote will be
    // ignored.
    bool in_escape = false;
    // If currently in a quoted string, this is the character that (when not
    // escaped) indicates the end of the string.
    char quote_close_char = '\0';
    // If currently in a quoted string, this is set to true if it is possible to
    // escape the closing quote using '\'.
    bool quote_allows_escape = false;

    while (token_end_ != end_) {
      char c = *token_end_;
      if (in_quote) {
        if (in_escape) {
          in_escape = false;
        } else if (quote_allows_escape && c == '\\') {
          in_escape = true;
        } else if (c == quote_close_char) {
          in_quote = false;
        }
      } else {
        if (c == ',')
          break;
        if (c == '\'' || c == '"' || c == '<') {
          in_quote = true;
          quote_close_char = (c == '<' ? '>' : c);
          quote_allows_escape = (c != '<');
        }
      }
      ++token_end_;
    }
    return true;
  }

  std::string::const_iterator token_begin_;
  std::string::const_iterator token_end_;
  std::string::const_iterator end_;
};

// Parses one link in a link header into its url and parameters.
// A link is of the form "<some-url>; param1=value1; param2=value2".
// Returns false if parsing the link failed, returns true on success. This
// method is more lenient than the RFC. It doesn't fail on things like invalid
// characters in the URL, and also doesn't verify that certain parameters should
// or shouldn't be quoted strings.
// If a parameter occurs more than once in the link, only the first value is
// returned in params as this is the required behavior for all attributes chrome
// currently cares about in link headers.
bool ParseLink(std::string::const_iterator begin,
               std::string::const_iterator end,
               std::string* url,
               std::unordered_map<std::string, std::string>* params) {
  // Can't parse an empty string.
  if (begin == end)
    return false;

  // Extract the URL part (everything between '<' and first '>' character).
  if (*begin != '<')
    return false;
  ++begin;
  std::string::const_iterator url_begin = begin;
  std::string::const_iterator url_end = std::find(begin, end, '>');
  // Fail if we did not find a '>'.
  if (url_end == end)
    return false;
  begin = url_end;
  net::HttpUtil::TrimLWS(&url_begin, &url_end);
  *url = std::string(url_begin, url_end);

  // Skip the '>' at the end of the URL, trim any remaining whitespace, and make
  // sure it is followed by a ';' to indicate the start of parameters.
  ++begin;
  net::HttpUtil::TrimLWS(&begin, &end);
  if (begin != end && *begin != ';')
    return false;

  // Parse all the parameters.
  net::HttpUtil::NameValuePairsIterator params_iterator(
      begin, end, ';', net::HttpUtil::NameValuePairsIterator::VALUES_OPTIONAL);
  while (params_iterator.GetNext()) {
    if (!net::HttpUtil::IsToken(params_iterator.name_begin(),
                                params_iterator.name_end()))
      return false;
    std::string name = base::ToLowerASCII(base::StringPiece(
        params_iterator.name_begin(), params_iterator.name_end()));
    params->insert(std::make_pair(name, params_iterator.value()));
  }
  return params_iterator.valid();
}

void RegisterServiceWorkerFinished(int64_t trace_id, bool result) {
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "LinkHeaderResourceThrottle::HandleServiceWorkerLink",
                         trace_id, "Success", result);
}

void HandleServiceWorkerLink(
    const net::URLRequest* request,
    const std::string& url,
    const std::unordered_map<std::string, std::string>& params,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    // TODO(mek): Integrate with experimental framework.
    return;
  }

  if (ContainsKey(params, "anchor"))
    return;

  const ResourceRequestInfoImpl* request_info =
      ResourceRequestInfoImpl::ForRequest(request);
  ResourceMessageFilter* filter = request_info->filter();
  ServiceWorkerContext* service_worker_context =
      filter ? filter->service_worker_context()
             : service_worker_context_for_testing;
  if (!service_worker_context)
    return;

  // TODO(mek): serviceworker links should only be supported on requests from
  // secure contexts. For now just check the initiator origin, even though that
  // is not correct: 1) the initiator isn't the origin that matters in case of
  // navigations, and 2) more than just a secure origin this needs to be a
  // secure context.
  if (!request->initiator().unique() &&
      !IsOriginSecure(GURL(request->initiator().Serialize())))
    return;

  // TODO(mek): support for a serviceworker link on a request that wouldn't ever
  // be able to be intercepted by a serviceworker isn't very useful, so this
  // should share logic with ServiceWorkerRequestHandler and
  // ForeignFetchRequestHandler to limit the requests for which serviceworker
  // links are processed.

  GURL context_url = request->url();
  GURL script_url = context_url.Resolve(url);
  auto scope_param = params.find("scope");
  GURL scope_url = scope_param == params.end()
                       ? script_url.Resolve("./")
                       : context_url.Resolve(scope_param->second);

  if (!context_url.is_valid() || !script_url.is_valid() ||
      !scope_url.is_valid())
    return;
  if (!ServiceWorkerUtils::CanRegisterServiceWorker(context_url, scope_url,
                                                    script_url))
    return;
  std::string error;
  if (ServiceWorkerUtils::ContainsDisallowedCharacter(scope_url, script_url,
                                                      &error))
    return;

  int render_process_id = -1;
  int render_frame_id = -1;
  ResourceRequestInfo::GetRenderFrameForRequest(request, &render_process_id,
                                                &render_frame_id);

  if (!GetContentClient()->browser()->AllowServiceWorker(
          scope_url, request->first_party_for_cookies(),
          request_info->GetContext(), render_process_id, render_frame_id))
    return;

  static int64_t trace_id = 0;
  TRACE_EVENT_ASYNC_BEGIN2(
      "ServiceWorker", "LinkHeaderResourceThrottle::HandleServiceWorkerLink",
      ++trace_id, "Pattern", scope_url.spec(), "Script URL", script_url.spec());
  service_worker_context->RegisterServiceWorker(
      scope_url, script_url,
      base::Bind(&RegisterServiceWorkerFinished, trace_id));
}

void ProcessLinkHeaderValueForRequest(
    const net::URLRequest* request,
    std::string::const_iterator value_begin,
    std::string::const_iterator value_end,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::string url;
  std::unordered_map<std::string, std::string> params;
  if (!ParseLink(value_begin, value_end, &url, &params))
    return;

  for (const auto& rel :
       base::SplitStringPiece(params["rel"], HTTP_LWS, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (base::EqualsCaseInsensitiveASCII(rel, "serviceworker"))
      HandleServiceWorkerLink(request, url, params,
                              service_worker_context_for_testing);
  }
}

}  // namespace

void ProcessRequestForLinkHeaders(const net::URLRequest* request) {
  std::string link_header;
  request->GetResponseHeaderByName("link", &link_header);
  if (link_header.empty())
    return;

  ProcessLinkHeaderForRequest(request, link_header);
}

void ProcessLinkHeaderForRequest(
    const net::URLRequest* request,
    const std::string& link_header,
    ServiceWorkerContextWrapper* service_worker_context_for_testing) {
  ValueTokenizer tokenizer(link_header.begin(), link_header.end());
  while (tokenizer.GetNext()) {
    ProcessLinkHeaderValueForRequest(request, tokenizer.token_begin(),
                                     tokenizer.token_end(),
                                     service_worker_context_for_testing);
  }
}

void SplitLinkHeaderForTesting(const std::string& header,
                               std::vector<std::string>* values) {
  values->clear();
  ValueTokenizer tokenizer(header.begin(), header.end());
  while (tokenizer.GetNext()) {
    values->push_back(
        std::string(tokenizer.token_begin(), tokenizer.token_end()));
  }
}

bool ParseLinkHeaderValueForTesting(
    const std::string& link,
    std::string* url,
    std::unordered_map<std::string, std::string>* params) {
  return ParseLink(link.begin(), link.end(), url, params);
}

}  // namespace content
