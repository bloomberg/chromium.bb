// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_security_policy/csp_context.h"

namespace content {

namespace {

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
    : allow_self(allow_self), allow_star(allow_star), sources(sources) {
  // When the '*' source is used, it must be the only one.
  DCHECK(!allow_star || (!allow_self && sources.empty()));
}

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
        url.SchemeIsWSOrWSS() || url.SchemeIs("ftp")) {
      return true;
    }
    if (context->self_source() && url.SchemeIs(context->self_source()->scheme))
      return true;
  }

  if (source_list.allow_self && context->self_source() &&
      CSPSource::Allow(context->self_source().value(), url, context,
                       is_redirect)) {
    return true;
  }

  return AllowFromSources(url, source_list.sources, context, is_redirect);
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
