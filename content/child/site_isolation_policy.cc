// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/site_isolation_policy.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/http/http_response_headers.h"
#include "third_party/WebKit/public/platform/WebHTTPHeaderVisitor.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

using WebKit::WebDocument;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLResponse;
using WebKit::WebURLRequest;

namespace content {

namespace {

// MIME types
const char kTextHtml[] = "text/html";
const char kTextXml[] = "text/xml";
const char xAppRssXml[] = "application/rss+xml";
const char kAppXml[] = "application/xml";
const char kAppJson[] = "application/json";
const char kTextJson[] = "text/json";
const char kTextXjson[] = "text/x-json";
const char kTextPlain[] = "text/plain";

}  // anonymous namespace

SiteIsolationPolicy::ResponseMetaData::ResponseMetaData() {}

void SiteIsolationPolicy::OnReceivedResponse(
    int request_id,
    GURL& frame_origin,
    GURL& response_url,
    ResourceType::Type resource_type,
    const webkit_glue::ResourceResponseInfo& info) {
  UMA_HISTOGRAM_COUNTS("SiteIsolation.AllResponses", 1);

  // See if this is for navigation. If it is, don't block it, under the
  // assumption that we will put it in an appropriate process.
  if (ResourceType::IsFrame(resource_type))
    return;

  if (!IsBlockableScheme(response_url))
    return;

  if (IsSameSite(frame_origin, response_url))
    return;

  SiteIsolationPolicy::ResponseMetaData::CanonicalMimeType canonical_mime_type =
      GetCanonicalMimeType(info.mime_type);

  if (canonical_mime_type == SiteIsolationPolicy::ResponseMetaData::Others)
    return;

  // Every CORS request should have the Access-Control-Allow-Origin header even
  // if it is preceded by a pre-flight request. Therefore, if this is a CORS
  // request, it has this header.  response.httpHeaderField() internally uses
  // case-insensitive matching for the header name.
  std::string access_control_origin;

  // We can use a case-insensitive header name for EnumerateHeader().
  info.headers->EnumerateHeader(
      NULL, "access-control-allow-origin", &access_control_origin);
  if (IsValidCorsHeaderSet(frame_origin, response_url, access_control_origin))
    return;

  // Real XSD data collection starts from here.
  std::string no_sniff;
  info.headers->EnumerateHeader(NULL, "x-content-type-options", &no_sniff);

  ResponseMetaData resp_data;
  resp_data.frame_origin = frame_origin.spec();
  resp_data.response_url = response_url;
  resp_data.resource_type = resource_type;
  resp_data.canonical_mime_type = canonical_mime_type;
  resp_data.http_status_code = info.headers->response_code();
  resp_data.no_sniff = LowerCaseEqualsASCII(no_sniff, "nosniff");

  RequestIdToMetaDataMap* metadata_map = GetRequestIdToMetaDataMap();
  (*metadata_map)[request_id] = resp_data;
}

// These macros are defined here so that we prevent code size bloat-up due to
// the UMA_HISTOGRAM_* macros. Similar logic is used for recording UMA stats for
// different MIME types, but we cannot create a helper function for this since
// UMA_HISTOGRAM_* macros do not accept variables as their bucket names. As a
// solution, macros are used instead to capture the repeated pattern for
// recording UMA stats.  TODO(dsjang): this is only needed for collecting UMA
// stat. Will be deleted when this class is used for actual blocking.

#define SITE_ISOLATION_POLICY_COUNT_BLOCK(BUCKET_PREFIX) \
    UMA_HISTOGRAM_COUNTS( BUCKET_PREFIX ".Blocked", 1); \
    result = true;                                      \
    if (renderable_status_code) { \
      UMA_HISTOGRAM_ENUMERATION( \
          BUCKET_PREFIX ".Blocked.RenderableStatusCode", \
        resp_data.resource_type, \
        WebURLRequest::TargetIsUnspecified + 1); \
    } else { \
      UMA_HISTOGRAM_COUNTS(BUCKET_PREFIX ".Blocked.NonRenderableStatusCode",1);\
    }

#define SITE_ISOLATION_POLICY_COUNT_NO_SNIFF_BLOCK(BUCKET_PREFIX) \
    UMA_HISTOGRAM_COUNTS( BUCKET_PREFIX ".NoSniffBlocked", 1); \
    result = true;  \
    if (renderable_status_code) { \
      UMA_HISTOGRAM_ENUMERATION( \
          BUCKET_PREFIX ".NoSniffBlocked.RenderableStatusCode", \
        resp_data.resource_type, \
        WebURLRequest::TargetIsUnspecified + 1); \
    } else { \
      UMA_HISTOGRAM_ENUMERATION( \
          BUCKET_PREFIX ".NoSniffBlocked.NonRenderableStatusCode", \
        resp_data.resource_type, \
        WebURLRequest::TargetIsUnspecified + 1); \
    }

#define SITE_ISOLATION_POLICY_COUNT_NOTBLOCK(BUCKET_PREFIX) \
    UMA_HISTOGRAM_COUNTS(BUCKET_PREFIX ".NotBlocked", 1); \
    if (is_sniffed_for_js) \
      UMA_HISTOGRAM_COUNTS(BUCKET_PREFIX ".NotBlocked.MaybeJS", 1); \

#define SITE_ISOLATION_POLICY_SNIFF_AND_COUNT(SNIFF_EXPR,BUCKET_PREFIX) \
  if (SNIFF_EXPR) { \
    SITE_ISOLATION_POLICY_COUNT_BLOCK(BUCKET_PREFIX) \
  } else { \
    if (resp_data.no_sniff) { \
      SITE_ISOLATION_POLICY_COUNT_NO_SNIFF_BLOCK(BUCKET_PREFIX) \
    } else { \
      SITE_ISOLATION_POLICY_COUNT_NOTBLOCK(BUCKET_PREFIX) \
    } \
  }

bool SiteIsolationPolicy::ShouldBlockResponse(
    int request_id,
    const char* data,
    int length,
    std::string* alternative_data) {
  RequestIdToMetaDataMap* metadata_map = GetRequestIdToMetaDataMap();
  RequestIdToResultMap* result_map = GetRequestIdToResultMap();

  // If there's an entry for |request_id| in blocked_map, this request's first
  // data packet has already been examined. We can return the result here.
  if (result_map->count(request_id) != 0) {
    if ((*result_map)[request_id]) {
      // Here, the blocking result has been set for the previous run of
      // ShouldBlockResponse(), so we set alternative data to an empty string so
      // that ResourceDispatcher doesn't call its peer's onReceivedData() with
      // the alternative data.
      alternative_data->erase();
      return true;
    }
    return false;
  }

  // If result_map doesn't have an entry for |request_id|, we're receiving the
  // first data packet for request_id. If request_id is not registered, this
  // request is identified as a non-target of our policy. So we return true.
  if (metadata_map->count(request_id) == 0) {
    // We set request_id to true so that we always return true for this request.
    (*result_map)[request_id] = false;
    return false;
  }

  // We now look at the first data packet received for request_id.
  ResponseMetaData resp_data = (*metadata_map)[request_id];
  metadata_map->erase(request_id);

  // Record the length of the first received network packet to see if it's
  // enough for sniffing.
  UMA_HISTOGRAM_COUNTS("SiteIsolation.XSD.DataLength", length);

  // Record the number of cross-site document responses with a specific mime
  // type (text/html, text/xml, etc).
  UMA_HISTOGRAM_ENUMERATION(
      "SiteIsolation.XSD.MimeType",
      resp_data.canonical_mime_type,
      SiteIsolationPolicy::ResponseMetaData::MaxCanonicalMimeType);

  // Store the result of cross-site document blocking analysis. True means we
  // can return this document to the renderer, false means that we have to block
  // the response data.
  bool result = false;

  // The content is blocked if it is sniffed for HTML/JSON/XML. When the blocked
  // response is with an error status code, it is not disruptive by the
  // following reasons : 1) the blocked content is not a binary object (such as
  // an image) since it is sniffed for text; 2) then, this blocking only breaks
  // the renderer behavior only if it is either JavaScript or CSS. However, the
  // renderer doesn't use the contents of JS/CSS with unaffected status code
  // (e.g, 404). 3) the renderer is expected not to use the cross-site document
  // content for purposes other than JS/CSS (e.g, XHR).
  bool renderable_status_code = IsRenderableStatusCodeForDocument(
      resp_data.http_status_code);

  // This is only used for false-negative analysis for non-blocked resources.
  bool is_sniffed_for_js = SniffForJS(data, length);

  // Record the number of responses whose content is sniffed for what its mime
  // type claims it to be. For example, we apply a HTML sniffer for a document
  // tagged with text/html here. Whenever this check becomes true, we'll block
  // the response.
  switch (resp_data.canonical_mime_type) {
    case SiteIsolationPolicy::ResponseMetaData::HTML:
      SITE_ISOLATION_POLICY_SNIFF_AND_COUNT(SniffForHTML(data, length),
                                            "SiteIsolation.XSD.HTML");
      break;
    case SiteIsolationPolicy::ResponseMetaData::XML:
      SITE_ISOLATION_POLICY_SNIFF_AND_COUNT(SniffForXML(data, length),
                                            "SiteIsolation.XSD.XML");
      break;
    case SiteIsolationPolicy::ResponseMetaData::JSON:
      SITE_ISOLATION_POLICY_SNIFF_AND_COUNT(SniffForJSON(data, length),
                                            "SiteIsolation.XSD.JSON");
      break;
    case SiteIsolationPolicy::ResponseMetaData::Plain:
      if (SniffForHTML(data, length)) {
        SITE_ISOLATION_POLICY_COUNT_BLOCK(
            "SiteIsolation.XSD.Plain.HTML");
      } else if (SniffForXML(data, length)) {
        SITE_ISOLATION_POLICY_COUNT_BLOCK(
            "SiteIsolation.XSD.Plain.XML");
      } else if (SniffForJSON(data, length)) {
        SITE_ISOLATION_POLICY_COUNT_BLOCK(
            "SiteIsolation.XSD.Plain.JSON");
      } else if (is_sniffed_for_js) {
        if (resp_data.no_sniff) {
          SITE_ISOLATION_POLICY_COUNT_NO_SNIFF_BLOCK(
              "SiteIsolation.XSD.Plain");
        } else {
          SITE_ISOLATION_POLICY_COUNT_NOTBLOCK(
              "SiteIsolation.XSD.Plain");
        }
      }
      break;
    default :
      NOTREACHED() <<
          "Not a blockable mime type. This mime type shouldn't reach here.";
      break;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kBlockCrossSiteDocuments))
    result = false;
  (*result_map)[request_id] = result;

  if (result) {
    alternative_data->erase();
    alternative_data->insert(0, " ");
    LOG(ERROR) << resp_data.response_url
               << " is blocked as an illegal cross-site document from "
               << resp_data.frame_origin;

  }
  return result;
}

#undef SITE_ISOLATION_POLICY_COUNT_NOTBLOCK
#undef SITE_ISOLATION_POLICY_SNIFF_AND_COUNT
#undef SITE_ISOLATION_POLICY_COUNT_BLOCK

void SiteIsolationPolicy::OnRequestComplete(int request_id) {
  RequestIdToMetaDataMap* metadata_map = GetRequestIdToMetaDataMap();
  RequestIdToResultMap* result_map = GetRequestIdToResultMap();
  metadata_map->erase(request_id);
  result_map->erase(request_id);
}

SiteIsolationPolicy::ResponseMetaData::CanonicalMimeType
SiteIsolationPolicy::GetCanonicalMimeType(const std::string& mime_type) {
  if (LowerCaseEqualsASCII(mime_type, kTextHtml)) {
    return SiteIsolationPolicy::ResponseMetaData::HTML;
  }

  if (LowerCaseEqualsASCII(mime_type, kTextPlain)) {
    return SiteIsolationPolicy::ResponseMetaData::Plain;
  }

  if (LowerCaseEqualsASCII(mime_type, kAppJson) ||
      LowerCaseEqualsASCII(mime_type, kTextJson) ||
      LowerCaseEqualsASCII(mime_type, kTextXjson)) {
    return SiteIsolationPolicy::ResponseMetaData::JSON;
  }

  if (LowerCaseEqualsASCII(mime_type, kTextXml) ||
      LowerCaseEqualsASCII(mime_type, xAppRssXml) ||
      LowerCaseEqualsASCII(mime_type, kAppXml)) {
    return SiteIsolationPolicy::ResponseMetaData::XML;
  }

 return SiteIsolationPolicy::ResponseMetaData::Others;

}

bool SiteIsolationPolicy::IsBlockableScheme(const GURL& url) {
  // We exclude ftp:// from here. FTP doesn't provide a Content-Type
  // header which our policy depends on, so we cannot protect any
  // document from FTP servers.
  return url.SchemeIs("http") || url.SchemeIs("https");
}

bool SiteIsolationPolicy::IsSameSite(const GURL& frame_origin,
                                     const GURL& response_url) {

  if (!frame_origin.is_valid() || !response_url.is_valid())
    return false;

  if (frame_origin.scheme() != response_url.scheme())
    return false;

  // SameDomainOrHost() extracts the effective domains (public suffix plus one)
  // from the two URLs and compare them.
  // TODO(dsjang): use INCLUDE_PRIVATE_REGISTRIES when http://crbug.com/7988 is
  // fixed.
  return net::registry_controlled_domains::SameDomainOrHost(
      frame_origin,
      response_url,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
}

bool SiteIsolationPolicy::IsFrameNavigating(WebKit::WebFrame* frame) {
  // When a navigation starts, frame->provisionalDataSource() is set
  // to a not-null value which stands for the request made for the
  // navigation. As soon as the network request is committed to the
  // frame, frame->provisionalDataSource() is converted to null, and
  // the committed data source is moved to frame->dataSource(). This
  // is the most reliable way to detect whether the frame is in
  // navigation or not.
  return frame->provisionalDataSource() != NULL;
}

// We don't use Webkit's existing CORS policy implementation since
// their policy works in terms of origins, not sites. For example,
// when frame is sub.a.com and it is not allowed to access a document
// with sub1.a.com. But under Site Isolation, it's allowed.
bool SiteIsolationPolicy::IsValidCorsHeaderSet(
    GURL& frame_origin,
    GURL& website_origin,
    std::string access_control_origin) {
  // Many websites are sending back "\"*\"" instead of "*". This is
  // non-standard practice, and not supported by Chrome. Refer to
  // CrossOriginAccessControl::passesAccessControlCheck().

  // TODO(dsjang): * is not allowed for the response from a request
  // with cookies. This allows for more than what the renderer will
  // eventually be able to receive, so we won't see illegal cross-site
  // documents allowed by this. We have to find a way to see if this
  // response is from a cookie-tagged request or not in the future.
  if (access_control_origin == "*")
    return true;

  // TODO(dsjang): The CORS spec only treats a fully specified URL, except for
  // "*", but many websites are using just a domain for access_control_origin,
  // and this is blocked by Webkit's CORS logic here :
  // CrossOriginAccessControl::passesAccessControlCheck(). GURL is set
  // is_valid() to false when it is created from a URL containing * in the
  // domain part.

  GURL cors_origin(access_control_origin);
  return IsSameSite(frame_origin, cors_origin);
}

// This function is a slight modification of |net::SniffForHTML|.
bool SiteIsolationPolicy::SniffForHTML(const char* data, size_t length) {
  // The content sniffer used by Chrome and Firefox are using "<!--"
  // as one of the HTML signatures, but it also appears in valid
  // JavaScript, considered as well-formed JS by the browser.  Since
  // we do not want to block any JS, we exclude it from our HTML
  // signatures. This can weaken our document block policy, but we can
  // break less websites.
  // TODO(dsjang): parameterize |net::SniffForHTML| with an option
  // that decides whether to include <!-- or not, so that we can
  // remove this function.
  const char* html_signatures[] = {"<!DOCTYPE html",  // HTML5 spec
                                   "<script",         // HTML5 spec, Mozilla
                                   "<html",           // HTML5 spec, Mozilla
                                   "<head",           // HTML5 spec, Mozilla
                                   "<iframe",         // Mozilla
                                   "<h1",             // Mozilla
                                   "<div",            // Mozilla
                                   "<font",           // Mozilla
                                   "<table",          // Mozilla
                                   "<a",              // Mozilla
                                   "<style",          // Mozilla
                                   "<title",          // Mozilla
                                   "<b",              // Mozilla
                                   "<body",           // Mozilla
                                   "<br", "<p",       // Mozilla
                                   "<?xml"            // Mozilla
  };

  if (MatchesSignature(
          data, length, html_signatures, arraysize(html_signatures)))
    return true;

  // "<!--" is specially treated since web JS can use "<!--" "-->" pair for
  // comments.
  const char* comment_begins[] = {"<!--" };

  if (MatchesSignature(
          data, length, comment_begins, arraysize(comment_begins))) {
    // Search for --> and do SniffForHTML after that. If we can find the
    // comment's end, we start HTML sniffing from there again.
    const char end_comment[] = "-->";
    const size_t end_comment_size = strlen(end_comment);

    for (size_t i = 0; i <= length - end_comment_size; ++i) {
      if (!strncmp(data + i, end_comment, end_comment_size)) {
        size_t skipped = i + end_comment_size;
        return SniffForHTML(data + skipped, length - skipped);
      }
    }
  }

  return false;
}

bool SiteIsolationPolicy::SniffForXML(const char* data, size_t length) {
  // TODO(dsjang): Chrome's mime_sniffer is using strncasecmp() for
  // this signature. However, XML is case-sensitive. Don't we have to
  // be more lenient only to block documents starting with the exact
  // string <?xml rather than <?XML ?
  const char* xml_signatures[] = {"<?xml"  // Mozilla
  };
  return MatchesSignature(
      data, length, xml_signatures, arraysize(xml_signatures));
}

bool SiteIsolationPolicy::SniffForJSON(const char* data, size_t length) {
  // TODO(dsjang): We have to come up with a better way to sniff
  // JSON. However, even RE cannot help us that much due to the fact
  // that we don't do full parsing.  This DFA starts with state 0, and
  // finds {, "/' and : in that order. We're avoiding adding a
  // dependency on a regular expression library.
  const int kInitState = 0;
  const int kLeftBraceState = 1;
  const int kLeftQuoteState = 2;
  const int kColonState = 3;
  const int kDeadState = 4;

  int state = kInitState;
  for (size_t i = 0; i < length && state < kColonState; ++i) {
    const char c = data[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      continue;

    switch (state) {
      case kInitState:
        if (c == '{')
          state = kLeftBraceState;
        else
          state = kDeadState;
        break;
      case kLeftBraceState:
        if (c == '\"' || c == '\'')
          state = kLeftQuoteState;
        else
          state = kDeadState;
        break;
      case kLeftQuoteState:
        if (c == ':')
          state = kColonState;
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return state == kColonState;
}

bool SiteIsolationPolicy::MatchesSignature(const char* raw_data,
                                           size_t raw_length,
                                           const char* signatures[],
                                           size_t arr_size) {
  size_t start = 0;
  // Skip white characters at the beginning of the document.
  for (start = 0; start < raw_length; ++start) {
    char c = raw_data[start];
    if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n'))
      break;
  }

  // There is no not-whitespace character in this document.
  if (!(start < raw_length))
    return false;

  const char* data = raw_data + start;
  size_t length = raw_length - start;

  for (size_t sig_index = 0; sig_index < arr_size; ++sig_index) {
    const char* signature = signatures[sig_index];
    size_t signature_length = strlen(signature);

    if (length < signature_length)
      continue;

    if (!base::strncasecmp(signature, data, signature_length))
      return true;
  }
  return false;
}

bool SiteIsolationPolicy::IsRenderableStatusCodeForDocument(int status_code) {
  // Chrome only uses the content of a response with one of these status codes
  // for CSS/JavaScript. For images, Chrome just ignores status code.
  const int renderable_status_code[] = {200, 201, 202, 203, 206, 300, 301, 302,
                                        303, 305, 306, 307};
  for (size_t i = 0; i < arraysize(renderable_status_code); ++i) {
    if (renderable_status_code[i] == status_code)
      return true;
  }
  return false;
}

bool SiteIsolationPolicy::SniffForJS(const char* data, size_t length) {
  // TODO(dsjang): This is a real hack. The only purpose of this function is to
  // try to see if there's any possibility that this data can be JavaScript
  // (superset of JS). This function will be removed once UMA stats are
  // gathered.

  // Search for "var " for JS detection.
  for (size_t i = 0; i < length - 3; ++i) {
    if (strncmp(data + i, "var ", 4) == 0)
      return true;
  }
  return false;
}

SiteIsolationPolicy::RequestIdToMetaDataMap*
SiteIsolationPolicy::GetRequestIdToMetaDataMap() {
  CR_DEFINE_STATIC_LOCAL(RequestIdToMetaDataMap, metadata_map_, ());
  return &metadata_map_;
}

SiteIsolationPolicy::RequestIdToResultMap*
SiteIsolationPolicy::GetRequestIdToResultMap() {
  CR_DEFINE_STATIC_LOCAL(RequestIdToResultMap, result_map_, ());
  return &result_map_;
}

}  // namespace content
