// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/chrome_web_client.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#import "base/ios/ios_util.h"
#import "base/ios/ns_error_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_about_rewriter.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/ios_chrome_main_parts.h"
#include "ios/chrome/browser/passwords/password_manager_features.h"
#import "ios/chrome/browser/reading_list/offline_page_tab_helper.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_error.h"
#import "ios/chrome/browser/safe_browsing/safe_browsing_unsafe_resource_container.h"
#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"
#import "ios/chrome/browser/ui/elements/windowed_container_view.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web/error_page_util.h"
#include "ios/chrome/browser/web/features.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/public/provider/chrome/browser/browser_url_rewriter_provider.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/voice/audio_session_controller.h"
#include "ios/public/provider/chrome/browser/voice/voice_search_provider.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/public/navigation/browser_url_rewriter.h"
#include "ios/web/public/navigation/navigation_manager.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}
// Returns the safe browsing error page HTML.
NSString* GetSafeBrowsingErrorPageHTML(web::WebState* web_state,
                                       int64_t navigation_id) {
  // Fetch the unsafe resource causing this error page from the WebState's
  // container.
  SafeBrowsingUnsafeResourceContainer* container =
      SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
  std::unique_ptr<security_interstitials::UnsafeResource> resource =
      container->ReleaseMainFrameUnsafeResource()
          ?: container->ReleaseSubFrameUnsafeResource(
                 web_state->GetNavigationManager()->GetLastCommittedItem());

  // Construct the blocking page and associate it with the WebState.
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage> page =
      SafeBrowsingBlockingPage::Create(*resource);
  std::string error_page_content = page->GetHtmlContents();
  security_interstitials::IOSBlockingPageTabHelper::FromWebState(web_state)
      ->AssociateBlockingPage(navigation_id, std::move(page));

  return base::SysUTF8ToNSString(error_page_content);
}
}  // namespace

ChromeWebClient::ChromeWebClient() {}

ChromeWebClient::~ChromeWebClient() {}

std::unique_ptr<web::WebMainParts> ChromeWebClient::CreateWebMainParts() {
  return std::make_unique<IOSChromeMainParts>(
      *base::CommandLine::ForCurrentProcess());
}

void ChromeWebClient::PreWebViewCreation() const {
  // TODO(crbug.com/1082371): Confirm that this code is no longer needed and
  // remove it entirely. Until then, prevent this from running on iOS 13.4+, as
  // it occasionally triggers a permissions prompt.
  if (!base::ios::IsRunningOnOrLater(13, 4, 0)) {
    // Initialize the audio session to allow a web page's audio to continue
    // playing after the app is backgrounded.
    VoiceSearchProvider* voice_provider =
        ios::GetChromeBrowserProvider()->GetVoiceSearchProvider();
    if (voice_provider) {
      AudioSessionController* audio_controller =
          voice_provider->GetAudioSessionController();
      if (audio_controller) {
        audio_controller->InitializeSessionIfNecessary();
      }
    }
  }
}

void ChromeWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

std::string ChromeWebClient::GetApplicationLocale() const {
  DCHECK(GetApplicationContext());
  return GetApplicationContext()->GetApplicationLocale();
}

bool ChromeWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

bool ChromeWebClient::ShouldBlockUrlDuringRestore(
    const GURL& url,
    web::WebState* web_state) const {
  return ios::GetChromeBrowserProvider()->ShouldBlockUrlDuringRestore(
      url, web_state);
}

void ChromeWebClient::AddSerializableData(
    web::SerializableUserDataManager* user_data_manager,
    web::WebState* web_state) {
  return ios::GetChromeBrowserProvider()->AddSerializableData(user_data_manager,
                                                              web_state);
}

base::string16 ChromeWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

std::string ChromeWebClient::GetUserAgent(web::UserAgentType type) const {
  // The user agent should not be requested for app-specific URLs.
  DCHECK_NE(type, web::UserAgentType::NONE);

  // Using desktop user agent overrides a command-line user agent, so that
  // request desktop site can still work when using an overridden UA.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (type != web::UserAgentType::DESKTOP &&
      command_line->HasSwitch(switches::kUserAgent)) {
    std::string user_agent =
        command_line->GetSwitchValueASCII(switches::kUserAgent);
    if (net::HttpUtil::IsValidHeaderValue(user_agent))
      return user_agent;
    LOG(WARNING) << "Ignored invalid value for flag --" << switches::kUserAgent;
  }

  return web::BuildUserAgentFromProduct(type, GetProduct());
}

base::string16 ChromeWebClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ChromeWebClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ChromeWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

void ChromeWebClient::GetAdditionalWebUISchemes(
    std::vector<std::string>* additional_schemes) {
  additional_schemes->push_back(dom_distiller::kDomDistillerScheme);
}

void ChromeWebClient::PostBrowserURLRewriterCreation(
    web::BrowserURLRewriter* rewriter) {
  rewriter->AddURLRewriter(&WillHandleWebBrowserAboutURL);
  BrowserURLRewriterProvider* provider =
      ios::GetChromeBrowserProvider()->GetBrowserURLRewriterProvider();
  if (provider)
    provider->AddProviderRewriters(rewriter);
}

NSString* ChromeWebClient::GetDocumentStartScriptForAllFrames(
    web::BrowserState* browser_state) const {
  return GetPageScript(@"chrome_bundle_all_frames");
}

NSString* ChromeWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  NSMutableArray* scripts = [NSMutableArray array];
  [scripts addObject:GetPageScript(@"chrome_bundle_main_frame")];

  if (base::FeatureList::IsEnabled(features::kCredentialManager)) {
    [scripts addObject:GetPageScript(@"credential_manager")];
  }

  return [scripts componentsJoinedByString:@";"];
}

void ChromeWebClient::AllowCertificateError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    int64_t navigation_id,
    const base::Callback<void(bool)>& callback) {
  base::OnceCallback<void(NSString*)> null_callback;
  // TODO(crbug.com/760873): IOSSSLErrorHandler will present an interstitial
  // for the user to decide if it is safe to proceed.
  // Handle the case of web_state not presenting UI to users like prerender tabs
  // or web_state used to fetch offline content in Reading List.
  IOSSSLErrorHandler::HandleSSLError(web_state, cert_error, info, request_url,
                                     overridable, navigation_id, callback,
                                     std::move(null_callback));
}

void ChromeWebClient::PrepareErrorPage(
    web::WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const base::Optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  OfflinePageTabHelper* offline_page_tab_helper =
      OfflinePageTabHelper::FromWebState(web_state);
  // WebState that are not attached to a tab may not have an
  // OfflinePageTabHelper.
  if (offline_page_tab_helper &&
      offline_page_tab_helper->HasDistilledVersionForOnlineUrl(url)) {
    // An offline version of the page will be displayed to replace this error
    // page. Loading an error page here can cause a race between the
    // navigation to load the error page and the navigation to display the
    // offline version of the page. If the latter navigation interrupts the
    // former and causes it to fail, this can incorrectly appear to be a
    // navigation back to the previous committed URL. To avoid this race,
    // return a nil error page here to avoid an error page load. See
    // crbug.com/980912.
    std::move(callback).Run(nil);
    return;
  }
  DCHECK(error);
  __block NSString* error_html = nil;
  __block base::OnceCallback<void(NSString*)> error_html_callback =
      std::move(callback);
  NSError* final_underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);
  if ([final_underlying_error.domain isEqual:kSafeBrowsingErrorDomain]) {
    // Only kUnsafeResourceErrorCode is supported.
    DCHECK_EQ(kUnsafeResourceErrorCode, final_underlying_error.code);
    std::move(error_html_callback)
        .Run(GetSafeBrowsingErrorPageHTML(web_state, navigation_id));
  } else if (info.has_value()) {
    base::OnceCallback<void(bool)> proceed_callback;
    base::OnceCallback<void(NSString*)> blocking_page_callback =
        base::BindOnce(^(NSString* blocking_page_html) {
          error_html = blocking_page_html;
          std::move(error_html_callback).Run(error_html);
        });
    IOSSSLErrorHandler::HandleSSLError(
        web_state, net::MapCertStatusToNetError(info.value().cert_status),
        info.value(), url, info.value().is_fatal_cert_error, navigation_id,
        std::move(proceed_callback), std::move(blocking_page_callback));
  } else {
    std::move(error_html_callback)
        .Run(GetErrorPage(url, error, is_post, is_off_the_record));
  }
}

UIView* ChromeWebClient::GetWindowedContainer() {
  if (!windowed_container_) {
    windowed_container_ = [[WindowedContainerView alloc] init];
  }
  return windowed_container_;
}

std::string ChromeWebClient::GetProduct() const {
  std::string product("CriOS/");
  product += version_info::GetVersionNumber();
  return product;
}

bool ChromeWebClient::ForceMobileVersionByDefault(const GURL& url) {
  DCHECK(web::features::UseWebClientDefaultUserAgent());
  if (base::FeatureList::IsEnabled(web::kMobileGoogleSRP)) {
    return google_util::IsGoogleSearchUrl(url);
  }
  return false;
}

web::UserAgentType ChromeWebClient::GetDefaultUserAgent(
    id<UITraitEnvironment> web_view,
    const GURL& url) {
  DCHECK(web::features::UseWebClientDefaultUserAgent());
  if (ForceMobileVersionByDefault(url))
    return web::UserAgentType::MOBILE;
  BOOL isRegularRegular = web_view.traitCollection.horizontalSizeClass ==
                              UIUserInterfaceSizeClassRegular &&
                          web_view.traitCollection.verticalSizeClass ==
                              UIUserInterfaceSizeClassRegular;
  return isRegularRegular ? web::UserAgentType::DESKTOP
                          : web::UserAgentType::MOBILE;
}

bool ChromeWebClient::IsEmbedderBlockRestoreUrlEnabled() {
  return ios::GetChromeBrowserProvider()->MightBlockUrlDuringRestore();
}
