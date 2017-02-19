// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/webstore_inline_installer.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace extensions {

const char kInvalidWebstoreResponseError[] =
    "Invalid Chrome Web Store response.";
const char kNoVerifiedSitesError[] =
    "Inline installs can only be initiated for Chrome Web Store items that "
    "have one or more verified sites.";
const char kNotFromVerifiedSitesError[] =
    "Installs can only be initiated by one of the Chrome Web Store item's "
    "verified sites.";
const char kInlineInstallSupportedError[] =
    "Inline installation is not supported for this item. The user will be "
    "redirected to the Chrome Web Store.";
const char kInitiatedFromPopupError[] =
    "Inline installs can not be initiated from pop-up windows.";
const char kInitiatedFromFullscreenError[] =
    "Inline installs can not be initiated from fullscreen.";

WebstoreInlineInstaller::WebstoreInlineInstaller(
    content::WebContents* web_contents,
    content::RenderFrameHost* host,
    const std::string& webstore_item_id,
    const GURL& requestor_url,
    const Callback& callback)
    : WebstoreStandaloneInstaller(
          webstore_item_id,
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          callback),
      content::WebContentsObserver(web_contents),
      host_(host),
      requestor_url_(requestor_url) {}

WebstoreInlineInstaller::~WebstoreInlineInstaller() {}

// static
bool WebstoreInlineInstaller::IsRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    const GURL& requestor_url,
    std::string* error) {
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
    requestor_is_ok = IsRequestorURLInVerifiedSite(requestor_url,
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
      if (IsRequestorURLInVerifiedSite(requestor_url, verified_site)) {
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

std::string WebstoreInlineInstaller::GetJsonPostData() {
  // web_contents() might return null during tab destruction. This object would
  // also be destroyed shortly thereafter but check to be on the safe side.
  if (!web_contents())
    return std::string();

  content::NavigationController& navigation_controller =
      web_contents()->GetController();
  content::NavigationEntry* navigation_entry =
      navigation_controller.GetLastCommittedEntry();

  if (navigation_entry) {
    const std::vector<GURL>& redirect_urls =
        navigation_entry->GetRedirectChain();

    if (!redirect_urls.empty()) {
      base::DictionaryValue dictionary;
      dictionary.SetString("id", id());
      dictionary.SetString("referrer", requestor_url_.spec());
      std::unique_ptr<base::ListValue> redirect_chain =
          base::MakeUnique<base::ListValue>();
      for (const GURL& url : redirect_urls) {
        redirect_chain->AppendString(url.spec());
      }
      dictionary.Set("redirect_chain", std::move(redirect_chain));

      std::string json;
      base::JSONWriter::Write(dictionary, &json);
      return json;
    }
  }

  return std::string();
}

bool WebstoreInlineInstaller::CheckRequestorAlive() const {
  // The frame or tab may have gone away - cancel installation in that case.
  return host_ != nullptr && web_contents() != nullptr &&
         chrome::FindBrowserWithWebContents(web_contents()) != nullptr;
}

const GURL& WebstoreInlineInstaller::GetRequestorURL() const {
  return requestor_url_;
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
WebstoreInlineInstaller::CreateInstallPrompt() const {
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::INLINE_INSTALL_PROMPT));

  // crbug.com/260742: Don't display the user count if it's zero. The reason
  // it's zero is very often that the number isn't actually being counted
  // (intentionally), which means that it's unlikely to be correct.
  prompt->SetWebstoreData(localized_user_count(),
                          show_user_count(),
                          average_rating(),
                          rating_count());
  return prompt;
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
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  DCHECK(browser);
  if (browser->is_type_popup()) {
    *error = kInitiatedFromPopupError;
    return false;
  }
  FullscreenController* controller =
      browser->exclusive_access_manager()->fullscreen_controller();
  if (controller->IsTabFullscreen()) {
    *error = kInitiatedFromFullscreenError;
    return false;
  }
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
    web_contents()->OpenURL(content::OpenURLParams(
        GURL(redirect_url),
        content::Referrer::SanitizeForRequest(
            GURL(redirect_url),
            content::Referrer(web_contents()->GetURL(),
                              blink::WebReferrerPolicyDefault)),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));
    *error = kInlineInstallSupportedError;
    return false;
  }
  *error = "";
  return true;
}

bool WebstoreInlineInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  return IsRequestorPermitted(webstore_data, requestor_url_, error);
}

//
// Private implementation.
//

void WebstoreInlineInstaller::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      !navigation_handle->IsSamePage() &&
      (navigation_handle->GetRenderFrameHost() == host_ ||
       navigation_handle->IsInMainFrame())) {
    host_ = nullptr;
  }
}

void WebstoreInlineInstaller::WebContentsDestroyed() {
  AbortInstall();
}

// static
bool WebstoreInlineInstaller::IsRequestorURLInVerifiedSite(
    const GURL& requestor_url,
    const std::string& verified_site) {
  // Turn the verified site into a URL that can be parsed by URLPattern.
  // |verified_site| must follow the format:
  //
  // [scheme://]host[:port][/path/specifier]
  //
  // If scheme is omitted, URLPattern will match against either an
  // HTTP or HTTPS requestor. If scheme is specified, it must be either HTTP
  // or HTTPS, and URLPattern will only match the scheme specified.
  GURL verified_site_url(verified_site);
  int valid_schemes = URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS;
  if (!verified_site_url.is_valid() || !verified_site_url.IsStandard())
    // If no scheme is specified, GURL will fail to parse the string correctly.
    // It will either determine that the URL is invalid, or parse a
    // host:port/path as scheme:host/path.
    verified_site_url = GURL("http://" + verified_site);
  else if (verified_site_url.SchemeIs("http"))
    valid_schemes = URLPattern::SCHEME_HTTP;
  else if (verified_site_url.SchemeIs("https"))
    valid_schemes = URLPattern::SCHEME_HTTPS;
  else
    return false;

  std::string port_spec =
      verified_site_url.has_port() ? ":" + verified_site_url.port() : "";
  std::string path_spec = verified_site_url.path() + "*";
  std::string verified_site_pattern_spec =
      base::StringPrintf(
          "%s://*.%s%s%s",
          verified_site_url.scheme().c_str(),
          verified_site_url.host().c_str(),
          port_spec.c_str(),
          path_spec.c_str());

  URLPattern verified_site_pattern(valid_schemes);
  URLPattern::ParseResult parse_result =
      verified_site_pattern.Parse(verified_site_pattern_spec);
  if (parse_result != URLPattern::PARSE_SUCCESS) {
    DLOG(WARNING) << "Could not parse " << verified_site_pattern_spec <<
        " as URL pattern " << parse_result;
    return false;
  }
  verified_site_pattern.SetScheme("*");

  return verified_site_pattern.MatchesURL(requestor_url);
}

}  // namespace extensions
