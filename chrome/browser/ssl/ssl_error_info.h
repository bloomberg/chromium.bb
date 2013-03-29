// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_INFO_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_INFO_H_

#include <string>
#include <vector>

#include "base/string16.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"

class GURL;

// This class describes an error that happened while showing a page over SSL.
// An SSLErrorInfo object only exists on the UI thread and only contains
// information about an error (type of error and text details).
// Note no DISALLOW_COPY_AND_ASSIGN as we want the copy constructor.
class SSLErrorInfo {
 public:
  // This enum is being histogrammed; please only add new values at the end.
  enum ErrorType {
    CERT_COMMON_NAME_INVALID = 0,
    CERT_DATE_INVALID,
    CERT_AUTHORITY_INVALID,
    CERT_CONTAINS_ERRORS,
    CERT_NO_REVOCATION_MECHANISM,
    CERT_UNABLE_TO_CHECK_REVOCATION,
    CERT_REVOKED,
    CERT_INVALID,
    CERT_WEAK_SIGNATURE_ALGORITHM,
    CERT_WEAK_KEY,
    UNKNOWN,
    END_OF_ENUM
  };

  virtual ~SSLErrorInfo();

  // Converts a network error code to an ErrorType.
  static ErrorType NetErrorToErrorType(int net_error);

  static SSLErrorInfo CreateError(ErrorType error_type,
                                  net::X509Certificate* cert,
                                  const GURL& request_url);

  // Populates the specified |errors| vector with the errors contained in
  // |cert_status|.  Returns the number of errors found.
  // Callers only interested in the error count can pass NULL for |errors|.
  // TODO(wtc): Document |cert_id| and |url| arguments.
  static int GetErrorsForCertStatus(int cert_id,
                                    net::CertStatus cert_status,
                                    const GURL& url,
                                    std::vector<SSLErrorInfo>* errors);

  // A title describing the error, usually to be used with the details below.
  const string16& title() const { return title_; }

  // A description of the error.
  const string16& details() const { return details_; }

  // A short message describing the error (1 line).
  const string16& short_description() const { return short_description_; }

  // A lengthy explanation of what the error is.  Each entry in the returned
  // vector is a paragraph.
  const std::vector<string16>& extra_information() const {
    return extra_information_;
  }

 private:
  SSLErrorInfo(const string16& title,
               const string16& details,
               const string16& short_description,
               const std::vector<string16>& extra_info);

  string16 title_;
  string16 details_;
  string16 short_description_;
  // Extra-informations contains paragraphs of text explaining in details what
  // the error is and what the risks are.
  std::vector<string16> extra_information_;
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_INFO_H_
