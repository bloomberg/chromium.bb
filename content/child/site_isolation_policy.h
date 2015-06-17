// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SITE_ISOLATION_POLICY_H_
#define CONTENT_CHILD_SITE_ISOLATION_POLICY_H_

#include <map>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "url/gurl.h"

namespace content {

struct ResourceResponseInfo;

// CrossSiteDocumentClassifier implements the cross-site document blocking
// policy (XSDP) for Site Isolation. XSDP will monitor network responses to a
// renderer and block illegal responses so that a compromised renderer cannot
// steal private information from other sites.
//
// SiteIsolationStatsGatherer monitors responses to gather various UMA stats to
// see the compatibility impact of actual deployment of the policy. The UMA stat
// categories SiteIsolationStatsGatherer gathers are as follows:
//
// SiteIsolation.AllResponses : # of all network responses.
// SiteIsolation.XSD.DataLength : the length of the first packet of a response.
// SiteIsolation.XSD.MimeType (enum):
//   # of responses from other sites, tagged with a document mime type.
//   0:HTML, 1:XML, 2:JSON, 3:Plain, 4:Others
// SiteIsolation.XSD.[%MIMETYPE].Blocked :
//   blocked # of cross-site document responses grouped by sniffed MIME type.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.RenderableStatusCode :
//   # of responses with renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].Blocked.NonRenderableStatusCode :
//   # of responses with non-renderable status code,
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.RenderableStatusCode :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with renderable status code
//   out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NoSniffBlocked.NonRenderableStatusCode :
//   # of responses failed to be sniffed for its MIME type, but blocked by
//   "X-Content-Type-Options: nosniff" header, and with non-renderable status
//   code out of SiteIsolation.XSD.[%MIMETYPE].Blocked.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked :
//   # of responses, but not blocked due to failure of mime sniffing.
// SiteIsolation.XSD.[%MIMETYPE].NotBlocked.MaybeJS :
//   # of responses that are plausibly sniffed to be JavaScript.

enum CrossSiteDocumentMimeType {
  // Note that these values are used in histograms, and must not change.
  CROSS_SITE_DOCUMENT_MIME_TYPE_HTML = 0,
  CROSS_SITE_DOCUMENT_MIME_TYPE_XML = 1,
  CROSS_SITE_DOCUMENT_MIME_TYPE_JSON = 2,
  CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN = 3,
  CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS = 4,
  CROSS_SITE_DOCUMENT_MIME_TYPE_MAX,
};

struct SiteIsolationResponseMetaData {
  SiteIsolationResponseMetaData();

  std::string frame_origin;
  GURL response_url;
  ResourceType resource_type;
  CrossSiteDocumentMimeType canonical_mime_type;
  int http_status_code;
  bool no_sniff;
};

// TODO(nick): Move this class into its own file.
class CONTENT_EXPORT SiteIsolationStatsGatherer {
 public:
  // Set activation flag for the UMA data collection for this renderer process.
  static void SetEnabled(bool enabled);

  // Returns any bookkeeping data about the HTTP header information for a
  // request. Any data returned should then be passed to OnReceivedFirstChunk()
  // with the first data chunk.
  static linked_ptr<SiteIsolationResponseMetaData> OnReceivedResponse(
      const GURL& frame_origin,
      const GURL& response_url,
      ResourceType resource_type,
      int origin_pid,
      const ResourceResponseInfo& info);

  // Examines the first chunk of network data in case response_url is registered
  // as a cross-site document by OnReceivedResponse(). This records various
  // kinds of UMA data stats. This function is called only if the length of
  // received data is non-zero.
  static bool OnReceivedFirstChunk(
      const linked_ptr<SiteIsolationResponseMetaData>& resp_data,
      const char* payload,
      int length);

 private:
  FRIEND_TEST_ALL_PREFIXES(SiteIsolationStatsGathererTest, SniffForJS);

  SiteIsolationStatsGatherer();  // Not instantiable.

  // Imprecise JS sniffing; only appropriate for collecting UMA stat.
  static bool SniffForJS(base::StringPiece data);

  DISALLOW_COPY_AND_ASSIGN(SiteIsolationStatsGatherer);
};

// TODO(nick): Move this class into its own file.
class CONTENT_EXPORT CrossSiteDocumentClassifier {
 public:
  // Returns the representative mime type enum value of the mime type of
  // response. For example, this returns the same value for all text/xml mime
  // type families such as application/xml, application/rss+xml.
  static CrossSiteDocumentMimeType GetCanonicalMimeType(
      const std::string& mime_type);

  // Returns whether this scheme is a target of cross-site document
  // policy(XSDP). This returns true only for http://* and https://* urls.
  static bool IsBlockableScheme(const GURL& frame_origin);

  // Returns whether the two urls belong to the same sites.
  static bool IsSameSite(const GURL& frame_origin, const GURL& response_url);

  // Returns whether there's a valid CORS header for frame_origin.  This is
  // simliar to CrossOriginAccessControl::passesAccessControlCheck(), but we use
  // sites as our security domain, not origins.
  // TODO(dsjang): this must be improved to be more accurate to the actual CORS
  // specification. For now, this works conservatively, allowing XSDs that are
  // not allowed by actual CORS rules by ignoring 1) credentials and 2)
  // methods. Preflight requests don't matter here since they are not used to
  // decide whether to block a document or not on the client side.
  static bool IsValidCorsHeaderSet(const GURL& frame_origin,
                                   const GURL& website_origin,
                                   const std::string& access_control_origin);

  static bool SniffForHTML(base::StringPiece data);
  static bool SniffForXML(base::StringPiece data);
  static bool SniffForJSON(base::StringPiece data);

 private:
  CrossSiteDocumentClassifier();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentClassifier);
};

}  // namespace content

#endif  // CONTENT_CHILD_SITE_ISOLATION_POLICY_H_
