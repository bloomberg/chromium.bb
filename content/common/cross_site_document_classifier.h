// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CROSS_SITE_DOCUMENT_CLASSIFIER_H_
#define CONTENT_COMMON_CROSS_SITE_DOCUMENT_CLASSIFIER_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

// CrossSiteDocumentClassifier implements the cross-site document blocking
// policy (XSDP) for Site Isolation. XSDP will monitor network responses to a
// renderer and block illegal responses so that a compromised renderer cannot
// steal private information from other sites.

enum CrossSiteDocumentMimeType {
  // Note that these values are used in histograms, and must not change.
  CROSS_SITE_DOCUMENT_MIME_TYPE_HTML = 0,
  CROSS_SITE_DOCUMENT_MIME_TYPE_XML = 1,
  CROSS_SITE_DOCUMENT_MIME_TYPE_JSON = 2,
  CROSS_SITE_DOCUMENT_MIME_TYPE_PLAIN = 3,
  CROSS_SITE_DOCUMENT_MIME_TYPE_OTHERS = 4,
  CROSS_SITE_DOCUMENT_MIME_TYPE_MAX,
};

class CONTENT_EXPORT CrossSiteDocumentClassifier {
 public:
  // Three conclusions are possible from sniffing a byte sequence:
  //  - No: meaning that the data definitively doesn't match the indicated type.
  //  - Yes: meaning that the data definitive does match the indicated type.
  //  - Maybe: meaning that if more bytes are appended to the stream, it's
  //    possible to get a Yes result. For example, if we are sniffing for a tag
  //    like "<html", a kMaybe result would occur if the data contains just
  //    "<ht".
  enum Result {
    kNo,
    kMaybe,
    kYes,
  };

  // Returns the representative mime type enum value of the mime type of
  // response. For example, this returns the same value for all text/xml mime
  // type families such as application/xml, application/rss+xml.
  static CrossSiteDocumentMimeType GetCanonicalMimeType(
      base::StringPiece mime_type);

  // Returns whether this scheme is a target of cross-site document
  // policy(XSDP). This returns true only for http://* and https://* urls.
  static bool IsBlockableScheme(const GURL& frame_origin);

  // Returns whether the two urls belong to the same sites.
  static bool IsSameSite(const url::Origin& frame_origin,
                         const GURL& response_url);

  // Returns whether there's a valid CORS header for frame_origin.  This is
  // simliar to CrossOriginAccessControl::passesAccessControlCheck(), but we use
  // sites as our security domain, not origins.
  // TODO(dsjang): this must be improved to be more accurate to the actual CORS
  // specification. For now, this works conservatively, allowing XSDs that are
  // not allowed by actual CORS rules by ignoring 1) credentials and 2)
  // methods. Preflight requests don't matter here since they are not used to
  // decide whether to block a document or not on the client side.
  static bool IsValidCorsHeaderSet(const url::Origin& frame_origin,
                                   const GURL& website_origin,
                                   const std::string& access_control_origin);

  static Result SniffForHTML(base::StringPiece data);
  static Result SniffForXML(base::StringPiece data);
  static Result SniffForJSON(base::StringPiece data);

  // Sniff for patterns that indicate |data| only ought to be consumed by XHR()
  // or fetch(). This detects Javascript parser-breaker and particular JS
  // infinite-loop patterns, which are used conventionally as a defense against
  // JSON data exfiltration by means of a <script> tag.
  static Result SniffForFetchOnlyResource(base::StringPiece data);

 private:
  CrossSiteDocumentClassifier();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(CrossSiteDocumentClassifier);
};

}  // namespace content

#endif  // CONTENT_COMMON_CROSS_SITE_DOCUMENT_CLASSIFIER_H_
