// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSITE_SETTINGS_H_
#define CHROME_BROWSER_WEBSITE_SETTINGS_H_

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/history/history.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class CertStore;
struct SSLStatus;
}

class HostContentSettingsMap;
class Profile;
class TabContentsWrapper;
class WebsiteSettingsUI;

// The |WebsiteSettings| provides information about a website's permissions,
// connection state and its identity. It owns a UI that displays the
// information and allows users to change the permissions. |WebsiteSettings|
// objects must be created on the heap. They destroy themselves after the UI is
// closed.
class WebsiteSettings : public TabSpecificContentSettings::SiteDataObserver {
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

  // Creates a |WebsiteSettingsModel| incl. a |WebsiteSettingsUI| and displays
  // the UI. The |url| contains the omnibox URL of the currently active tab,
  // |parent| contains the currently active window, |profile| contains the
  // currently active profile and |ssl| contains the |SSLStatus| of the
  // connection to the website in the currently active tab that is wrapped by
  // the |tab_contents_wrapper|.
  static void Show(gfx::NativeWindow parent,
                   Profile* profile,
                   TabContentsWrapper* tab_contents_wrapper,
                   const GURL& url,
                   const content::SSLStatus& ssl);

  // Creates a WebsiteSettings for the passed |url| using the given |ssl| status
  // object to determine the status of the site's connection. The
  // |WebsiteSettings| takes ownership of the |ui|.
  WebsiteSettings(WebsiteSettingsUI* ui,
                  Profile* profile,
                  TabSpecificContentSettings* tab_specific_content_settings,
                  const GURL& url,
                  const content::SSLStatus& ssl,
                  content::CertStore* cert_store);

  // This method is called when the WebsiteSettingsUI is closed. It triggers
  // the destruction of this |WebsiteSettings| object. So it is not safe to use
  // this object afterwards. Any pointers to this object will be invalid.
  void OnUIClosing();

  // This method is called when ever a permission setting is changed.
  void OnSitePermissionChanged(ContentSettingsType type,
                               ContentSetting value);

  // Callback used for requests to fetch the number of page visits from history
  // service and the time of the first visit.
  void OnGotVisitCountToHost(HistoryService::Handle handle,
                             bool found_visits,
                             int visit_count,
                             base::Time first_visit);

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

  // SiteDataObserver implementation.
  virtual void OnSiteDataAccessed() OVERRIDE;

 private:
  virtual ~WebsiteSettings();

  // Initializes the |WebsiteSettings|.
  void Init(Profile* profile,
            const GURL& url,
            const content::SSLStatus& ssl);

  // Sets (presents) the information about the site's permissions in the |ui_|.
  void PresentSitePermissions();

  // Sets (presents) the information about the site's data in the |ui_|.
  void PresentSiteData();

  // Sets (presents) the information about the site's identity and connection
  // in the |ui_|.
  void PresentSiteIdentity();

  // Sets (presents) history information about the site in the |ui_|. Passing
  // base::Time() as value for |first_visit| will clear the history information
  // in the UI.
  void PresentHistoryInfo(base::Time first_visit);

  // The website settings UI displays information and controls for site
  // specific data (local stored objects like cookies), site specific
  // permissions (location, popup, plugin, etc.  permissions) and site specific
  // information (identity, connection status, etc.).
  scoped_ptr<WebsiteSettingsUI> ui_;

  // The Omnibox URL of the website for which to display site permissions and
  // site information.
  GURL site_url_;

  // Status of the website's identity verification check.
  SiteIdentityStatus site_identity_status_;

  // For secure connection |cert_id_| is set to the ID of the server
  // certificate. For non secure connections |cert_id_| is 0.
  int cert_id_;

  // Status of the connection to the website.
  SiteConnectionStatus site_connection_status_;

  // TODO(markusheintz): Move the creation of all the string16 typed UI
  // strings below to the corresponding UI code, in order to prevent
  // unnecessary UTF-8 string conversions.

  // Details about the website's identity. If the website's identity has been
  // verified then |site_identity_details_| contains who verified the identity.
  // This string will be displayed in the UI.
  string16 site_identity_details_;

  // Details about the connection to the website. In case of an encrypted
  // connection |site_connection_details_| contains encryption details, like
  // encryption strength and ssl protocol version. This string will be
  // displayed in the UI.
  string16 site_connection_details_;

  // For websites that provided an EV certificate |orgainization_name_|
  // contains the organization name of the certificate. In all other cases
  // |organization_name| is an empty string. This string will be displayed in
  // the UI.
  string16 organization_name_;

  // The |CertStore| provides all X509Certificates.
  content::CertStore* cert_store_;

  // The |HostContentSettingsMap| is the service that provides and manages
  // content settings (aka. site permissions).
  HostContentSettingsMap* content_settings_;

  // Used to request the number of page visits.
  CancelableRequestConsumer visit_count_request_consumer_;

  DISALLOW_COPY_AND_ASSIGN(WebsiteSettings);
};

#endif  // CHROME_BROWSER_WEBSITE_SETTINGS_H_
