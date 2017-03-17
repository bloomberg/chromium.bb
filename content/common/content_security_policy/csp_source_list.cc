// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

const GURL ExtractInnerURL(const GURL& url) {
  if (const GURL* inner_url = url.inner_url())
    return *inner_url;
  else
    // TODO(arthursonzogni): revisit this once GURL::inner_url support blob-URL.
    return GURL(url.path());
}

const GURL GetEffectiveURL(CSPContext* context, const GURL& url) {
  // Due to backwards-compatibility concerns, we allow 'self' to match blob and
  // filesystem inner URLs if we are in a context that bypasses
  // ContentSecurityPolicy in the main world.
  if (context->SelfSchemeShouldBypassCSP()) {
    if (url.SchemeIsFileSystem() || url.SchemeIsBlob())
      return ExtractInnerURL(url);
  }
  return url;
}

bool AllowFromSources(const GURL& url,
                      const std::vector<CSPSource>& sources,
                      CSPContext* context,
                      bool is_redirect) {
  for (const CSPSource& source : sources) {
    if (CSPSource::Allow(source, url, context, is_redirect))
      return true;
  }
  return false;
}

};  // namespace

CSPSourceList::CSPSourceList()
    : allow_self(false), allow_star(false), sources() {}

CSPSourceList::CSPSourceList(bool allow_self,
                             bool allow_star,
                             std::vector<CSPSource> sources)
    : allow_self(allow_self), allow_star(allow_star), sources(sources) {}

CSPSourceList::CSPSourceList(const CSPSourceList&) = default;
CSPSourceList::~CSPSourceList() = default;

// static
bool CSPSourceList::Allow(const CSPSourceList& source_list,
                          const GURL& url,
                          CSPContext* context,
                          bool is_redirect) {
  // Wildcards match network schemes ('http', 'https', 'ftp', 'ws', 'wss'), and
  // the scheme of the protected resource:
  // https://w3c.github.io/webappsec-csp/#match-url-to-source-expression. Other
  // schemes, including custom schemes, must be explicitly listed in a source
  // list.
  if (source_list.allow_star) {
    if (url.SchemeIsHTTPOrHTTPS() || url.SchemeIsSuborigin() ||
        url.SchemeIsWSOrWSS() || url.SchemeIs("ftp") ||
        context->ProtocolMatchesSelf(url))
      return true;

    return AllowFromSources(url, source_list.sources, context, is_redirect);
  }

  const GURL effective_url = GetEffectiveURL(context, url);

  if (source_list.allow_self && context->AllowSelf(effective_url))
    return true;

  return AllowFromSources(effective_url, source_list.sources, context,
                          is_redirect);
}

std::string CSPSourceList::ToString() const {
  if (IsNone())
    return "'none'";
  if (allow_star)
    return "*";

  bool is_empty = true;
  std::stringstream text;
  if (allow_self) {
    text << "'self'";
    is_empty = false;
  }

  for (const auto& source : sources) {
    if (!is_empty)
      text << " ";
    text << source.ToString();
    is_empty = false;
  }

  return text.str();
}

bool CSPSourceList::IsNone() const {
  return !allow_self && !allow_star && sources.empty();
}

}  // namespace content
