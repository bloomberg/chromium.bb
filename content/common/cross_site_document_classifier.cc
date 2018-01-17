// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cross_site_document_classifier.h"

#include <stddef.h>
#include <string>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_response_info.h"

using base::StringPiece;

namespace content {

namespace {

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kImageSvg[] = "image/svg+xml";
const char kTextJson[] = "text/json";
const char kTextXjson[] = "text/x-json";
const char kTextPlain[] = "text/plain";

// MIME type suffixes
const char kJsonSuffix[] = "+json";
const char kXmlSuffix[] = "+xml";

void AdvancePastWhitespace(StringPiece* data) {
  size_t offset = data->find_first_not_of(" \t\r\n");
  if (offset == base::StringPiece::npos) {
    // |data| was entirely whitespace.
    data->clear();
  } else {
    data->remove_prefix(offset);
  }
}

// Returns kYes if |data| starts with one of the string patterns in
// |signatures|, kMaybe if |data| is a prefix of one of the patterns in
// |signatures|, and kNo otherwise.
//
// When kYes is returned, the matching prefix is erased from |data|.
CrossSiteDocumentClassifier::Result MatchesSignature(
    StringPiece* data,
    const StringPiece signatures[],
    size_t arr_size,
    base::CompareCase compare_case) {
  for (size_t i = 0; i < arr_size; ++i) {
    if (signatures[i].length() <= data->length()) {
      if (base::StartsWith(*data, signatures[i], compare_case)) {
        // When |signatures[i]| is a prefix of |data|, it constitutes a match.
        // Strip the matching characters, and return.
        data->remove_prefix(signatures[i].length());
        return CrossSiteDocumentClassifier::kYes;
      }
    } else {
      if (base::StartsWith(signatures[i], *data, compare_case)) {
        // When |data| is a prefix of |signatures[i]|, that means that
        // subsequent bytes in the stream could cause a match to occur.
        return CrossSiteDocumentClassifier::kMaybe;
      }
    }
  }
  return CrossSiteDocumentClassifier::kNo;
}

// Returns true if |mime_type == prefix| or if |mime_type| starts with
// |prefix + '+'|.  Returns false otherwise.
//
// For example:
// - MatchesMimeTypePrefix("application/json", "application/json") -> true
// - MatchesMimeTypePrefix("application/json+foo", "application/json") -> true
// - MatchesMimeTypePrefix("application/jsonp", "application/json") -> false
// - MatchesMimeTypePrefix("application/foo", "application/json") -> false
bool MatchesMimeTypePrefix(base::StringPiece mime_type,
                           base::StringPiece prefix) {
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (!base::StartsWith(mime_type, prefix, kCaseInsensitive))
    return false;
  DCHECK_GE(mime_type.length(), prefix.length());

  if (mime_type.length() == prefix.length()) {
    // Given StartsWith results above, the above condition is our O(1) check if
    // |base::LowerCaseEqualsASCII(mime_type, prefix)|.
    DCHECK(base::LowerCaseEqualsASCII(mime_type, prefix));
    return true;
  }

  if (mime_type[prefix.length()] == '+') {
    // Given StartsWith results above, the above condition is our O(1) check if
    // |base::StartsWith(mime_type, prefix + '+', kCaseInsensitive)|.
    DCHECK(base::StartsWith(mime_type, prefix.as_string() + '+',
                            kCaseInsensitive));
    return true;
  }

  return false;
}

}  // namespace

CrossSiteDocumentMimeType CrossSiteDocumentClassifier::GetCanonicalMimeType(
    base::StringPiece mime_type) {
  // Checking for image/svg+xml early ensures that it won't get classified as
  // CROSS_SITE_DOCUMENT_MIME_TYPE_XML by the presence of the "+xml" suffix.
  if (base::LowerCaseEqualsASCII(mime_type, kImageSvg))
    return CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS;

  if (base::LowerCaseEqualsASCII(mime_type, kTextHtml))
    return CROSS_SITE_DOCUMENT_MIME_TYPE_HTML;

  if (base::LowerCaseEqualsASCII(mime_type, kTextPlain))
    return CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN;

  // StartsWith rather than LowerCaseEqualsASCII is used to account both for
  // mime types similar to 1) application/json and to 2)
  // application/json+protobuf.
  constexpr auto kCaseInsensitive = base::CompareCase::INSENSITIVE_ASCII;
  if (MatchesMimeTypePrefix(mime_type, kAppJson) ||
      MatchesMimeTypePrefix(mime_type, kTextJson) ||
      MatchesMimeTypePrefix(mime_type, kTextXjson) ||
      base::EndsWith(mime_type, kJsonSuffix, kCaseInsensitive)) {
    return CROSS_SITE_DOCUMENT_MIME_TYPE_JSON;
  }

  if (MatchesMimeTypePrefix(mime_type, kAppXml) ||
      MatchesMimeTypePrefix(mime_type, kTextXml) ||
      base::EndsWith(mime_type, kXmlSuffix, kCaseInsensitive)) {
    return CROSS_SITE_DOCUMENT_MIME_TYPE_XML;
  }

  return CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS;
}

bool CrossSiteDocumentClassifier::IsBlockableScheme(const GURL& url) {
  // We exclude ftp:// from here. FTP doesn't provide a Content-Type
  // header which our policy depends on, so we cannot protect any
  // document from FTP servers.
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme);
}

bool CrossSiteDocumentClassifier::IsSameSite(const url::Origin& frame_origin,
                                             const GURL& response_url) {
  if (frame_origin.unique() || !response_url.is_valid())
    return false;

  if (frame_origin.scheme() != response_url.scheme())
    return false;

  // SameDomainOrHost() extracts the effective domains (public suffix plus one)
  // from the two URLs and compare them.
  return net::registry_controlled_domains::SameDomainOrHost(
      response_url, frame_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// We don't use Webkit's existing CORS policy implementation since
// their policy works in terms of origins, not sites. For example,
// when frame is sub.a.com and it is not allowed to access a document
// with sub1.a.com. But under Site Isolation, it's allowed.
bool CrossSiteDocumentClassifier::IsValidCorsHeaderSet(
    const url::Origin& frame_origin,
    const GURL& website_origin,
    const std::string& access_control_origin) {
  // Many websites are sending back "\"*\"" instead of "*". This is
  // non-standard practice, and not supported by Chrome. Refer to
  // CrossOriginAccessControl::passesAccessControlCheck().

  // Note that "null" offers no more protection than "*" because it matches any
  // unique origin, such as data URLs. Any origin can thus access it, so don't
  // bother trying to block this case.

  // TODO(dsjang): * is not allowed for the response from a request
  // with cookies. This allows for more than what the renderer will
  // eventually be able to receive, so we won't see illegal cross-site
  // documents allowed by this. We have to find a way to see if this
  // response is from a cookie-tagged request or not in the future.
  if (access_control_origin == "*" || access_control_origin == "null")
    return true;

  return IsSameSite(frame_origin, GURL(access_control_origin));
}

// This function is a slight modification of |net::SniffForHTML|.
CrossSiteDocumentClassifier::Result CrossSiteDocumentClassifier::SniffForHTML(
    StringPiece data) {
  // The content sniffers used by Chrome and Firefox are using "<!--" as one of
  // the HTML signatures, but it also appears in valid JavaScript, considered as
  // well-formed JS by the browser.  Since we do not want to block any JS, we
  // exclude it from our HTML signatures. This can weaken our document block
  // policy, but we can break less websites.
  //
  // Note that <body> and <br> are not included below, since <b is a prefix of
  // them.
  //
  // TODO(dsjang): parameterize |net::SniffForHTML| with an option that decides
  // whether to include <!-- or not, so that we can remove this function.
  // TODO(dsjang): Once CrossSiteDocumentClassifier is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  static const StringPiece kHtmlSignatures[] = {
      StringPiece("<!doctype html"),  // HTML5 spec
      StringPiece("<script"),         // HTML5 spec, Mozilla
      StringPiece("<html"),           // HTML5 spec, Mozilla
      StringPiece("<head"),           // HTML5 spec, Mozilla
      StringPiece("<iframe"),         // Mozilla
      StringPiece("<h1"),             // Mozilla
      StringPiece("<div"),            // Mozilla
      StringPiece("<font"),           // Mozilla
      StringPiece("<table"),          // Mozilla
      StringPiece("<a"),              // Mozilla
      StringPiece("<style"),          // Mozilla
      StringPiece("<title"),          // Mozilla
      StringPiece("<b"),              // Mozilla (note: subsumes <body>, <br>)
      StringPiece("<p")               // Mozilla
  };

  while (data.length() > 0) {
    AdvancePastWhitespace(&data);

    Result signature_match =
        MatchesSignature(&data, kHtmlSignatures, arraysize(kHtmlSignatures),
                         base::CompareCase::INSENSITIVE_ASCII);
    if (signature_match != kNo)
      return signature_match;

    // "<!--" (the HTML comment syntax) is a special case, since it's valid JS
    // as well. Skip over them.
    static const StringPiece kBeginCommentSignature[] = {"<!--"};
    Result comment_match = MatchesSignature(&data, kBeginCommentSignature,
                                            arraysize(kBeginCommentSignature),
                                            base::CompareCase::SENSITIVE);
    if (comment_match != kYes)
      return comment_match;

    // Look for an end comment.
    static const StringPiece kEndComment = "-->";
    size_t comment_end = data.find(kEndComment);
    if (comment_end == base::StringPiece::npos)
      return kMaybe;  // Hit end of data with open comment.
    data.remove_prefix(comment_end + kEndComment.length());
  }

  // All of |data| was consumed, without a clear determination.
  return kMaybe;
}

CrossSiteDocumentClassifier::Result CrossSiteDocumentClassifier::SniffForXML(
    base::StringPiece data) {
  // TODO(dsjang): Once CrossSiteDocumentClassifier is moved into the browser
  // process, we should do single-thread checking here for the static
  // initializer.
  AdvancePastWhitespace(&data);
  static const StringPiece kXmlSignatures[] = {StringPiece("<?xml")};
  return MatchesSignature(&data, kXmlSignatures, arraysize(kXmlSignatures),
                          base::CompareCase::SENSITIVE);
}

CrossSiteDocumentClassifier::Result CrossSiteDocumentClassifier::SniffForJSON(
    base::StringPiece data) {
  // Currently this function looks for an opening brace ('{'), followed by a
  // double-quoted string literal, followed by a colon. Importantly, such a
  // sequence is a Javascript syntax error: although the JSON object syntax is
  // exactly Javascript's object-initializer syntax, a Javascript object-
  // initializer expression is not valid as a standalone Javascript statement.
  //
  // TODO(nick): We have to come up with a better way to sniff JSON. The
  // following are known limitations of this function:
  // https://crbug.com/795470/ Support non-dictionary values (e.g. lists)
  enum {
    kStartState,
    kLeftBraceState,
    kLeftQuoteState,
    kEscapeState,
    kRightQuoteState,
  } state = kStartState;

  for (size_t i = 0; i < data.length(); ++i) {
    const char c = data[i];
    if (state != kLeftQuoteState && state != kEscapeState) {
      // Whitespace is ignored (outside of string literals)
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        continue;
    } else {
      // Inside string literals, control characters should result in rejection.
      if ((c >= 0 && c < 32) || c == 127)
        return kNo;
    }

    switch (state) {
      case kStartState:
        if (c == '{')
          state = kLeftBraceState;
        else
          return kNo;
        break;
      case kLeftBraceState:
        if (c == '"')
          state = kLeftQuoteState;
        else
          return kNo;
        break;
      case kLeftQuoteState:
        if (c == '"')
          state = kRightQuoteState;
        else if (c == '\\')
          state = kEscapeState;
        break;
      case kEscapeState:
        // Simplification: don't bother rejecting hex escapes.
        state = kLeftQuoteState;
        break;
      case kRightQuoteState:
        if (c == ':')
          return kYes;
        else
          return kNo;
        break;
    }
  }
  return kMaybe;
}

CrossSiteDocumentClassifier::Result
CrossSiteDocumentClassifier::SniffForFetchOnlyResource(base::StringPiece data) {
  // kScriptBreakingPrefixes contains prefixes that are conventionally used to
  // prevent a JSON response from becoming a valid Javascript program (an attack
  // vector known as XSSI). The presence of such a prefix is a strong signal
  // that the resource is meant to be consumed only by the fetch API or
  // XMLHttpRequest, and is meant to be protected from use in non-CORS, cross-
  // origin contexts like <script>, <img>, etc.
  //
  // These prefixes work either by inducing a syntax error, or inducing an
  // infinite loop. In either case, the prefix must create a guarantee that no
  // matter what bytes follow it, the entire response would be worthless to
  // execute as a <script>.
  static const StringPiece kScriptBreakingPrefixes[] = {
      // Parser breaker prefix.
      //
      // Built into angular.js (followed by a comma and a newline):
      //   https://docs.angularjs.org/api/ng/service/$http
      //
      // Built into the Java Spring framework (followed by a comma and a space):
      //   https://goo.gl/xP7FWn
      //
      // Observed on google.com (without a comma, followed by a newline).
      StringPiece(")]}'"),

      // Apache struts: https://struts.apache.org/plugins/json/#prefix
      StringPiece("{}&&"),

      // Spring framework (historically): https://goo.gl/JYPFAv
      StringPiece("{} &&"),

      // Infinite loops.
      StringPiece("for(;;);"),  // observed on facebook.com
      StringPiece("while(1);"), StringPiece("for (;;);"),
      StringPiece("while (1);"),
  };
  Result has_parser_breaker = MatchesSignature(
      &data, kScriptBreakingPrefixes, arraysize(kScriptBreakingPrefixes),
      base::CompareCase::SENSITIVE);
  if (has_parser_breaker != kNo)
    return has_parser_breaker;

  // A non-empty JSON object also effectively introduces a JS syntax error.
  return SniffForJSON(data);
}

}  // namespace content
