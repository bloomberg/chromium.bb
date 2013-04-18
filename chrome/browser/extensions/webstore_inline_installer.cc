// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_inline_installer.h"

#include "base/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

const char kVerifiedSiteKey[] = "verified_site";
const char kVerifiedSitesKey[] = "verified_sites";
const char kInlineInstallNotSupportedKey[] = "inline_install_not_supported";
const char kRedirectUrlKey[] = "redirect_url";

const char kInvalidWebstoreResponseError[] = "Invalid Chrome Web Store reponse";
const char kNoVerifiedSitesError[] =
    "Inline installs can only be initiated for Chrome Web Store items that "
    "have one or more verified sites";
const char kNotFromVerifiedSitesError[] =
    "Installs can only be initiated by one of the Chrome Web Store item's "
    "verified sites";
const char kInlineInstallSupportedError[] =
    "Inline installation is not supported for this item. The user will be "
    "redirected to the Chrome Web Store.";

WebstoreInlineInstaller::WebstoreInlineInstaller(
    content::WebContents* web_contents,
    const std::string& webstore_item_id,
    const GURL& requestor_url,
    const Callback& callback)
    : WebstoreStandaloneInstaller(
          webstore_item_id,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          callback),
      content::WebContentsObserver(web_contents),
      requestor_url_(requestor_url) {
}

WebstoreInlineInstaller::~WebstoreInlineInstaller() {}

bool WebstoreInlineInstaller::CheckRequestorAlive() const {
  // The tab may have gone away - cancel installation in that case.
  return web_contents() != NULL;
}

const GURL& WebstoreInlineInstaller::GetRequestorURL() const {
  return requestor_url_;
}

scoped_ptr<ExtensionInstallPrompt::Prompt>
WebstoreInlineInstaller::CreateInstallPrompt() const {
  scoped_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::INLINE_INSTALL_PROMPT));
  prompt->SetInlineInstallWebstoreData(localized_user_count(),
                                      average_rating(),
                                      rating_count());
  return prompt.Pass();
}

bool WebstoreInlineInstaller::ShouldShowPostInstallUI() const {
  return true;
}

bool WebstoreInlineInstaller::ShouldShowAppInstalledBubble() const {
  return true;
}

WebContents* WebstoreInlineInstaller::GetWebContents() const {
  return web_contents();
}

bool WebstoreInlineInstaller::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // The store may not support inline installs for this item, in which case
  // we open the store-provided redirect URL in a new tab and abort the
  // installation process.
  bool inline_install_not_supported = false;
  if (webstore_data.HasKey(kInlineInstallNotSupportedKey)
      && !webstore_data.GetBoolean(kInlineInstallNotSupportedKey,
                                    &inline_install_not_supported)) {
    *error = kInvalidWebstoreResponseError;
    return false;
  }
  if (inline_install_not_supported) {
    std::string redirect_url;
    if (!webstore_data.GetString(kRedirectUrlKey, &redirect_url)) {
      *error = kInvalidWebstoreResponseError;
      return false;
    }
    web_contents()->OpenURL(
        content::OpenURLParams(
            GURL(redirect_url),
            content::Referrer(web_contents()->GetURL(),
                              WebKit::WebReferrerPolicyDefault),
            NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    *error = kInlineInstallSupportedError;
    return false;
  }

  *error = "";
  return true;
}

bool WebstoreInlineInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  // Ensure that there is at least one verified site present.
  const bool data_has_single_site = webstore_data.HasKey(kVerifiedSiteKey);
  const bool data_has_site_list = webstore_data.HasKey(kVerifiedSitesKey);
  if (!data_has_single_site && !data_has_site_list) {
    *error = kNoVerifiedSitesError;
    return false;
  }
  bool requestor_is_ok = false;
  // Handle the deprecated single-site case.
  if (!data_has_site_list) {
    std::string verified_site;
    if (!webstore_data.GetString(kVerifiedSiteKey, &verified_site)) {
      *error = kInvalidWebstoreResponseError;
      return false;
    }
    requestor_is_ok = IsRequestorURLInVerifiedSite(requestor_url_,
                                                   verified_site);
  } else {
    const base::ListValue* verified_sites = NULL;
    if (!webstore_data.GetList(kVerifiedSitesKey, &verified_sites)) {
      *error = kInvalidWebstoreResponseError;
      return false;
    }
    for (base::ListValue::const_iterator it = verified_sites->begin();
         it != verified_sites->end() && !requestor_is_ok; ++it) {
      std::string verified_site;
      if (!(*it)->GetAsString(&verified_site)) {
        *error = kInvalidWebstoreResponseError;
        return false;
      }
      if (IsRequestorURLInVerifiedSite(requestor_url_, verified_site)) {
        requestor_is_ok = true;
      }
    }
  }
  if (!requestor_is_ok) {
    *error = kNotFromVerifiedSitesError;
    return false;
  }
  *error = "";
  return true;
}

//
// Private implementation.
//

void WebstoreInlineInstaller::WebContentsDestroyed(
    content::WebContents* web_contents) {
  AbortInstall();
}

// static
bool WebstoreInlineInstaller::IsRequestorURLInVerifiedSite(
    const GURL& requestor_url,
    const std::string& verified_site) {
  // Turn the verified site (which may be a bare domain, or have a port and/or a
  // path) into a URL that can be parsed by URLPattern.
  std::string verified_site_url =
      base::StringPrintf(
          "http://*.%s%s",
          verified_site.c_str(),
          verified_site.find('/') == std::string::npos ? "/*" : "*");

  URLPattern verified_site_pattern(
      URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS);
  URLPattern::ParseResult parse_result =
      verified_site_pattern.Parse(verified_site_url);
  if (parse_result != URLPattern::PARSE_SUCCESS) {
    DLOG(WARNING) << "Could not parse " << verified_site_url <<
        " as URL pattern " << parse_result;
    return false;
  }
  verified_site_pattern.SetScheme("*");

  return verified_site_pattern.MatchesURL(requestor_url);
}

}  // namespace extensions
