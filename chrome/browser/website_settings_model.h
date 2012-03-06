// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSITE_SETTINGS_MODEL_H_
#define CHROME_BROWSER_WEBSITE_SETTINGS_MODEL_H_

#include "base/string16.h"
#include "googleurl/src/gurl.h"

namespace content {
struct SSLStatus;
}

class CertStore;
class Profile;
class TabContentsWrapper;

// The |WebsiteSettingsModel| provides information about the connection and the
// identity of a website. The |WebsiteSettingsModel| is the backend for the
// WebsiteSettingsUI which displays this information.
class WebsiteSettingsModel {
 public:
  // Status of a connection to a website.
  enum SiteConnectionStatus {
    SITE_CONNECTION_STATUS_UNKNOWN = 0,      // No status available.
    SITE_CONNECTION_STATUS_ENCRYPTED,        // Connection is encrypted.
    SITE_CONNECTION_STATUS_MIXED_CONTENT,    // Site has unencrypted content.
    SITE_CONNECTION_STATUS_UNENCRYPTED,      // Connection is not encrypted.
    SITE_CONNECTION_STATUS_ENCRYPTED_ERROR,  // Connection error occured.
    SITE_CONNECTION_STATUS_INTERNAL_PAGE,    // Internal site.
  };

  // Validation status of a website's identity.
  enum SiteIdentityStatus {
    // No status about the website's identity available.
    SITE_IDENTITY_STATUS_UNKNOWN = 0,
    // The website provided a valid certificate.
    SITE_IDENTITY_STATUS_CERT,
    // The website provided a valid EV certificate.
    SITE_IDENTITY_STATUS_EV_CERT,
    // The website provided a valid DNSSEC certificate.
    SITE_IDENTITY_STATUS_DNSSEC_CERT,
    // The website provided a valid certificate but no revocation check could be
    // performed.
    SITE_IDENTITY_STATUS_CERT_REVOCATION_UNKNOWN,
    // Site identity could not be verified because the site did not provide a
    // certificate. This is the expected state for HTTP connections.
    SITE_IDENTITY_STATUS_NO_CERT,
    // An error occured while verifying the site identity.
    SITE_IDENTITY_STATUS_ERROR,
    // The site is a trusted internal chrome page.
    SITE_IDENTITY_STATUS_INTERNAL_PAGE,
  };

  // Creates a WebsiteSettingsModel for the passed |url| using the given |ssl|
  // status object to determine the status of the site's connection.
  WebsiteSettingsModel(Profile* profile,
                       const GURL& url,
                       const content::SSLStatus& ssl,
                       CertStore* cert_store);

  virtual ~WebsiteSettingsModel();

  // Accessors.
  SiteConnectionStatus site_connection_status() const {
    return site_connection_status_;
  }

  SiteIdentityStatus site_identity_status() const {
    return site_identity_status_;
  }

  string16 site_connection_details() const {
    return site_connection_details_;
  }

  string16 site_identity_details() const {
    return site_identity_details_;
  }

  string16 organization_name() const {
    return organization_name_;
  }

 private:
  // Initializes the |WebsiteSettingsModel|.
  void Init(Profile* profile,
            const GURL& url,
            const content::SSLStatus& ssl);

  // Status of the website's identity verification check.
  SiteIdentityStatus site_identity_status_;

  // Status of the connection to the website.
  SiteConnectionStatus site_connection_status_;

  // Details about the website's identity. If the website's identity has been
  // verified then |site_identity_details_| contains who verified the identity.
  string16 site_identity_details_;

  // Details about the connection to the website. In case of an encrypted
  // connection |site_connection_details_| contains encryption details, like
  // encryption strength and ssl protocol version.
  string16 site_connection_details_;

  // For websites that provided an EV certificate |orgainization_name_| contains
  // the organization name of the certificate. In all other cases
  // |organization_name| is an empty string.
  string16 organization_name_;

  // The |CertStore| provides all X509Certificates.
  CertStore* cert_store_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettingsModel);
};

#endif  // CHROME_BROWSER_WEBSITE_SETTINGS_MODEL_H_
