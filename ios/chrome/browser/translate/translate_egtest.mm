// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/language/ios/browser/ios_language_detection_tab_helper.h"
#include "components/strings/grit/components_strings.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_switches.h"
#include "components/translate/ios/browser/ios_translate_driver.h"
#import "components/translate/ios/browser/js_translate_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/ui/translate/language_selection_view_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/earl_grey/js_test_util.h"
#include "ios/web/public/test/http_server/data_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Some text in English language.
const char kEnglishText[] =
    "After flying to an altitude of 39,045 meters (128,100 feet) in a "
    "helium-filled balloon, Felix Baumgartner completed a record breaking jump "
    "for the ages from the edge of space, exactly 65 years after Chuck Yeager "
    "first broke the sound barrier flying in an experimental rocket-powered "
    "airplane. Felix reached a maximum of speed of 1,342.8 km/h (833mph) "
    "through the near vacuum of the stratosphere before being slowed by the "
    "atmosphere later during his 4:20 minute long freefall. The 43-year-old "
    "Austrian skydiving expert also broke two other world records (highest "
    "freefall, highest manned balloon flight), leaving the one for the longest "
    "freefall to project mentor Col. Joe Kittinger.";

// Some text in French language.
const char kFrenchText[] =
    "Des yeux qui font baisser les miens. Un rire qui se perd sur sa bouche."
    "Voilà le portrait sans retouches de l'homme auquel j'appartiens "
    "Quand il me prend dans ses bras Il me parle tout bas "
    "Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est lui pour moi, moi pour lui, dans la vie "
    "Il me l'a dit, l'a juré, pour la vie Et dès que je l'aperçois "
    "Alors je sens en moi, Mon cœur qui bat Des nuits d'amour à plus finir "
    "Un grand bonheur qui prend sa place Les ennuis, les chagrins s'effacent "
    "Heureux, heureux à en mourir Quand il me prend dans ses bras "
    "Il me parle tout bas Je vois la vie en rose Il me dit des mots d'amour "
    "Des mots de tous les jours Et ça me fait quelque chose "
    "Il est entré dans mon cœur Une part de bonheur Dont je connais la cause "
    "C'est toi pour moi, moi pour toi, dans la vie "
    "Tu me l'as dit, l'as juré, pour la vie Et dès que je t'aperçois "
    "Alors je sens en moi Mon cœur qui bat";

// Various HTML tags.
const char kHtmlAttribute[] = "<html>";
const char kHtmlAttributeWithDeLang[] = "<html lang=\"de\">";
const char kMetaNotranslateContent[] =
    "<meta name=\"google\" content=\"notranslate\">";
const char kMetaNotranslateValue[] =
    "<meta name=\"google\" value=\"notranslate\">";
const char kMetaItContentLanguage[] =
    "<meta http-equiv=\"content-language\" content=\"it\">";

// Various link components.
// TODO(crbug.com/729195): Re-write the hardcoded address.
const char kHttpServerDomain[] = "127.0.0.1";
const char kLanguagePath[] = "/languagepath/";
const char kLinkPath[] = "/linkpath/";
const char kSubresourcePath[] = "/subresourcepath/";
const char kSomeLanguageUrl[] = "http://languagepath/?http=es";
const char kFrenchPagePath[] = "/frenchpage/";
const char kFrenchPageWithLinkPath[] = "/frenchpagewithlink/";
const char kTranslateScriptPath[] = "/translatescript/";
const char kTranslateScript[] = "Fake Translate Script";

// Body text for /languagepath/.
const char kLanguagePathText[] = "Some text here.";

// Builds a HTML document with a French text and the given |html| and |meta|
// tags.
std::string GetFrenchPageHtml(const std::string& html_tag,
                              const std::string& meta_tags) {
  return html_tag + meta_tags + "<body>" + kFrenchText + "</body></html>";
}

// Assigns the testing callback for the current WebState's language detection
// helper.
void SetTestingLanguageDetectionCallback(
    const language::IOSLanguageDetectionTabHelper::Callback& callback) {
  language::IOSLanguageDetectionTabHelper::FromWebState(
      chrome_test_util::GetCurrentWebState())
      ->SetExtraCallbackForTesting(callback);
}

#pragma mark - TestResponseProvider

// A ResponseProvider that provides html responses of texts in different
// languages or links.
class TestResponseProvider : public web::DataResponseProvider {
 public:
  // TestResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;

 private:
  // Generates a page with a HTTP "Content-Language" header and "httpEquiv" meta
  // tag.
  // The URL in |request| has two parameters, "http" and "meta", that can be
  // used to set the values of the header and the meta tag. For example:
  // http://someurl?http=en&meta=fr generates a page with a "en" HTTP header and
  // a "fr" meta tag.
  void GetLanguageResponse(const Request& request,
                           scoped_refptr<net::HttpResponseHeaders>* headers,
                           std::string* response_body);
};

bool TestResponseProvider::CanHandleRequest(const Request& request) {
  const GURL& url = request.url;
  return url.host() == kHttpServerDomain &&
         (url.path() == kLanguagePath || url.path() == kLinkPath ||
          url.path() == kSubresourcePath || url.path() == kFrenchPagePath ||
          url.path() == kFrenchPageWithLinkPath ||
          url.path() == kTranslateScriptPath);
}

void TestResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  *headers = web::ResponseProvider::GetDefaultResponseHeaders();
  if (url.path() == kLanguagePath) {
    // HTTP header and meta tag read from parameters.
    return GetLanguageResponse(request, headers, response_body);
  } else if (url.path() == kSubresourcePath) {
    // Different "Content-Language" headers in the main page and subresource.
    (*headers)->AddHeader("Content-Language: fr");
    *response_body = base::StringPrintf(
        "<html><body><img src=%s></body></html>", kSomeLanguageUrl);
    return;
  } else if (url.path() == kLinkPath) {
    // Link to a page with "Content Language" headers.
    GURL url = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
    *response_body = base::StringPrintf(
        "<html><body><a href='%s' id='click'>Click</a></body></html>",
        url.spec().c_str());
    return;
  } else if (url.path() == kFrenchPagePath) {
    *response_body =
        base::StringPrintf("<html><body>%s</body></html>", kFrenchText);
    return;
  } else if (url.path() == kFrenchPageWithLinkPath) {
    GURL page_path_url = web::test::HttpServer::MakeUrl(
        base::StringPrintf("http://%s", kFrenchPagePath));
    *response_body = base::StringPrintf(
        "<html><body>%s<br /><a href='%s' id='link'>link</a></body></html>",
        kFrenchText, page_path_url.spec().c_str());
    return;
  } else if (url.path() == kTranslateScriptPath) {
    *response_body = kTranslateScript;
    return;
  }
  NOTREACHED();
}

void TestResponseProvider::GetLanguageResponse(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  const GURL& url = request.url;
  // HTTP headers.
  std::string http;
  net::GetValueForKeyInQuery(url, "http", &http);
  if (!http.empty())
    (*headers)->AddHeader(std::string("Content-Language: ") + http);
  // Response body.
  std::string meta;
  net::GetValueForKeyInQuery(url, "meta", &meta);
  *response_body = "<html>";
  if (!meta.empty()) {
    *response_body +=
        "<head>"
        "<meta http-equiv='content-language' content='" +
        meta +
        "'>"
        "</head>";
  }
  *response_body +=
      base::StringPrintf("<html><body>%s</body></html>", kLanguagePathText);
}

// Simulates a given network connection type for tests.
// TODO(crbug.com/938598): Refactor this and similar net::NetworkChangeNotifier
// subclasses for testing into a separate file.
class FakeNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  FakeNetworkChangeNotifier(
      net::NetworkChangeNotifier::ConnectionType connection_type_to_return)
      : connection_type_to_return_(connection_type_to_return) {}

 private:
  ConnectionType GetCurrentConnectionType() const override {
    return connection_type_to_return_;
  }

  // The currently simulated network connection type. If this is set to
  // CONNECTION_NONE, then NetworkChangeNotifier::IsOffline will return true.
  net::NetworkChangeNotifier::ConnectionType connection_type_to_return_ =
      net::NetworkChangeNotifier::CONNECTION_UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(FakeNetworkChangeNotifier);
};

}  // namespace

using chrome_test_util::TapWebViewElementWithId;
using translate::LanguageDetectionController;

#pragma mark - MockTranslateScriptManager

// Mock javascript translate manager that does not use the translate servers.
// Translating the page just adds a 'Translated' button to the page, without
// changing the text.
@interface MockTranslateScriptManager : JsTranslateManager {
  web::WebState* _webState;  // weak
}

- (instancetype)initWithWebState:(web::WebState*)webState;

@end

@implementation MockTranslateScriptManager

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    _webState = webState;
  }
  return self;
}

- (void)setScript:(NSString*)script {
}

- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target {
  // Add a button with the 'Translated' label to the web page.
  // The test can check it to determine if this method has been called.
  _webState->ExecuteJavaScript(base::UTF8ToUTF16(
      "myButton = document.createElement('button');"
      "myButton.appendChild(document.createTextNode('Translated'));"
      "document.body.appendChild(myButton);"));
}

- (void)inject {
  // Prevent the actual script from being injected and instead just invoke host
  // with 'translate.ready' followed by 'translate.status'.
  _webState->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.invokeOnHost({"
                        "  'command': 'translate.ready',"
                        "  'errorCode': 0,"
                        "  'loadTime': 0,"
                        "  'readyTime': 0});"));
  _webState->ExecuteJavaScript(
      base::UTF8ToUTF16("__gCrWeb.message.invokeOnHost({"
                        "  'command': 'translate.status',"
                        "  'errorCode': 0,"
                        "  'originalPageLanguage': 'fr',"
                        "  'translationTime': 0});"));
}

@end

#pragma mark - TranslateTestCase

// Tests for translate.
@interface TranslateTestCase : ChromeTestCase {
  std::unique_ptr<translate::LanguageDetectionDetails>
      _language_detection_details;
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest>
      network_change_notifier_disabler_;
  std::unique_ptr<FakeNetworkChangeNotifier> network_change_notifier_;
}
@end

@implementation TranslateTestCase

- (void)setUp {
  [super setUp];
  language::IOSLanguageDetectionTabHelper::Callback copyDetailsCallback =
      base::BindRepeating(^(
          const translate::LanguageDetectionDetails& details) {
        _language_detection_details =
            std::make_unique<translate::LanguageDetectionDetails>(details);
      });
  SetTestingLanguageDetectionCallback(copyDetailsCallback);

  // Disable the net::NetworkChangeNotifier singleton and replace it with a
  // FakeNetworkChangeNotifier to simulate a WIFI network connection.
  network_change_notifier_disabler_ =
      std::make_unique<net::NetworkChangeNotifier::DisableForTest>();
  network_change_notifier_ = std::make_unique<FakeNetworkChangeNotifier>(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
}

- (void)tearDown {
  SetTestingLanguageDetectionCallback(
      language::IOSLanguageDetectionTabHelper::Callback());
  _language_detection_details.reset();
  // TODO(crbug.com/642892): Investigate moving into test-specific teardown.
  // Re-enable translate.
  chrome_test_util::SetBooleanUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      prefs::kOfferTranslateEnabled, YES);
  // Reset translate prefs to default.
  std::unique_ptr<translate::TranslatePrefs> translatePrefs(
      ChromeIOSTranslateClient::CreateTranslatePrefs(
          chrome_test_util::GetOriginalBrowserState()->GetPrefs()));
  translatePrefs->ResetToDefaults();
  translate::TranslateManager::SetIgnoreMissingKeyForTesting(false);
  [super tearDown];
}

#pragma mark - Test Cases

// Tests that different language signals are detected correcty.
- (void)testLanguageDetection {
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://scenarioLanguageDetection");
  std::map<GURL, std::string> responses;
  // A page with French text, German "lang" attribute and Italian content
  // language.
  responses[URL] =
      GetFrenchPageHtml(kHtmlAttributeWithDeLang, kMetaItContentLanguage);
  web::test::SetUpSimpleHttpServer(responses);

  translate::LanguageDetectionDetails expectedLanguageDetails;
  expectedLanguageDetails.content_language = "it";
  expectedLanguageDetails.html_root_language = "de";
  expectedLanguageDetails.adopted_language = translate::kUnknownLanguageCode;

  [ChromeEarlGrey loadURL:URL];
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that hidden text is not considered during detection.
- (void)testLanguageDetectionIgnoreHiddenText {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionIgnoreHiddenText");
  std::map<GURL, std::string> responses;
  // A page with French text that's hidden via CSS.
  responses[URL] = base::StringPrintf(
      "<html><body style='display:none'>%s</body></html>", kFrenchText);
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  translate::LanguageDetectionDetails expectedLanguageDetails;
  expectedLanguageDetails.adopted_language = translate::kUnknownLanguageCode;
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language detection is not performed when the page specifies that
// it should not be translated.
- (void)testLanguageDetectionNoTranslate {
  const GURL noTranslateContentURL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionNoTranslate_content");
  const GURL noTranslateValueURL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionNoTranslate_value");
  std::map<GURL, std::string> responses;
  // A page with French text and a 'content' attribute with "notranslate".
  responses[noTranslateContentURL] =
      GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateContent);
  // A page with French text and a 'value' attribute with "notranslate".
  responses[noTranslateValueURL] =
      GetFrenchPageHtml(kHtmlAttribute, kMetaNotranslateValue);
  web::test::SetUpSimpleHttpServer(responses);

  // Load some french page with |content="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateContentURL];

  // Check that no language has been detected.
  GREYAssert(_language_detection_details.get() == nullptr,
             @"A language has been detected");

  // Load some french page with |value="notranslate"| meta tag.
  [ChromeEarlGrey loadURL:noTranslateValueURL];

  // Check that no language has been detected.
  GREYAssert(_language_detection_details.get() == nullptr,
             @"A language has been detected");
}

// Tests that history.pushState triggers a new detection.
- (void)testLanguageDetectionWithPushState {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionPushState");
  std::map<GURL, std::string> responses;
  // Page without meaningful text, language should be undefined ("und").
  responses[URL] = "<html><body>Blahrg :)</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check for no language detected.
  translate::LanguageDetectionDetails expectedLanguageDetails;
  expectedLanguageDetails.adopted_language = "und";
  [self assertLanguageDetails:expectedLanguageDetails];
  // Change the text of the page.
  chrome_test_util::ExecuteJavaScript(
      [NSString stringWithFormat:@"document.write('%s');", kEnglishText], nil);
  // Trigger a new detection with pushState.
  chrome_test_util::ExecuteJavaScript(@"history.pushState(null, null, null);",
                                      nil);
  // Check that the new language has been detected.
  expectedLanguageDetails.adopted_language = "en";
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language detection is performed on hash changes.
- (void)testLanguageDetectionWithHashChange {
  // Generate a page with French text and a button that changes the text to
  // English and triggers a hash change.
  std::string html = base::StringPrintf(
      "<html><head><script>"
      "function hash() {"
      "  document.getElementById('text').innerHTML = '%s';"
      "  location.href='#1';"
      "}"
      "</script></head><body>"
      "<input type='button' value='Hash' id='Hash' onclick='hash()'>"
      "<div id='text'>%s</div>"
      "</body></html>",
      kEnglishText, kFrenchText);

  // Set up the mock server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://hashChangeLanguageDetected");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:URL];
  // Check that language has been detected.
  translate::LanguageDetectionDetails expectedLanguageDetails;
  expectedLanguageDetails.adopted_language = "fr";
  [self assertLanguageDetails:expectedLanguageDetails];
  // Trigger the hash change.
  GREYAssert(TapWebViewElementWithId("Hash"), @"Failed to tap \"Hash\"");
  // Check that language detection has been re-run.
  expectedLanguageDetails.adopted_language = "en";
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language in http content is detected.
- (void)testLanguageDetectionHttpContentLanguage {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));
  translate::LanguageDetectionDetails expectedLanguageDetails;

  // The HTTP header is detected.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") +
                                            kLanguagePath + "?http=fr");
  [ChromeEarlGrey loadURL:URL];
  expectedLanguageDetails.content_language = "fr";
  expectedLanguageDetails.adopted_language = "fr";
  [self assertLanguageDetails:expectedLanguageDetails];

  // Everything after the comma is truncated.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr,ornot");
  [ChromeEarlGrey loadURL:URL];
  expectedLanguageDetails.content_language = "fr";
  expectedLanguageDetails.adopted_language = "fr";
  [self assertLanguageDetails:expectedLanguageDetails];

  // The HTTP header is overriden by meta tag.
  URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLanguagePath +
                                       "?http=fr&meta=it");
  [ChromeEarlGrey loadURL:URL];
  expectedLanguageDetails.content_language = "it";
  expectedLanguageDetails.adopted_language = "it";
  [self assertLanguageDetails:expectedLanguageDetails];

  // Only the header of the main page is detected.
  URL =
      web::test::HttpServer::MakeUrl(std::string("http://") + kSubresourcePath);
  [ChromeEarlGrey loadURL:URL];
  expectedLanguageDetails.content_language = "fr";
  expectedLanguageDetails.adopted_language = "fr";
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language in http content is detected when navigating to a link.
- (void)testLanguageDetectionHttpContentLanguageBehindLink {
  // Start the HTTP server.
  std::unique_ptr<web::DataResponseProvider> provider(new TestResponseProvider);
  web::test::SetUpHttpServer(std::move(provider));
  translate::LanguageDetectionDetails expectedLanguageDetails;

  // Detection works when clicking on a link.
  GURL URL = web::test::HttpServer::MakeUrl(std::string("http://") + kLinkPath);
  GURL someLanguageURL = web::test::HttpServer::MakeUrl(kSomeLanguageUrl);
  [ChromeEarlGrey loadURL:URL];
  GREYAssert(TapWebViewElementWithId("click"), @"Failed to tap \"click\"");
  [ChromeEarlGrey waitForWebViewContainingText:kLanguagePathText];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          someLanguageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  expectedLanguageDetails.content_language = "es";
  expectedLanguageDetails.adopted_language = "es";
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language detection still happens when a very large quantity of
// text is present on the page.
- (void)testLanguageDetectionLargePage {
  // Generate very large page.
  std::string html = "<html lang='fr'><body>";
  NSUInteger targetSize = 1024 * 1024;  // More than 1 MB of page content.
  while (html.length() < targetSize) {
    html.append(kFrenchText);
  }
  html.append("</body></html>");

  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://languageDetectionLargePage");
  responses[URL] = html;
  web::test::SetUpSimpleHttpServer(responses);

  if (@available(iOS 12, *)) {
    // TODO(crbug.com/874452) iOS12 has a bug where long pages take forever to
    // load.  Add a 20 seconds timeout here.
    chrome_test_util::LoadUrl(URL);
    web::WebState* webState = chrome_test_util::GetCurrentWebState();
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                   20,
                   ^{
                     return !webState->IsLoading();
                   }),
               @"Failed to load large page on iOS 12.");
    if (webState->ContentIsHTML())
      web::WaitUntilWindowIdInjected(webState);
  } else {
    [ChromeEarlGrey loadURL:URL];
  }

  // Check that language has been detected.
  translate::LanguageDetectionDetails expectedLanguageDetails;
  expectedLanguageDetails.html_root_language = "fr";
  expectedLanguageDetails.adopted_language = "fr";
  [self assertLanguageDetails:expectedLanguageDetails];
}

// Tests that language detection is not performed when translate is disabled.
- (void)testLanguageDetectionDisabled {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://scenarioLanguageDetectionDisabled");
  std::map<GURL, std::string> responses;
  // A page with some text.
  responses[URL] = "<html><body>Hello world!</body></html>";
  web::test::SetUpSimpleHttpServer(responses);

  // Disable translate.
  chrome_test_util::SetBooleanUserPref(
      chrome_test_util::GetOriginalBrowserState(),
      prefs::kOfferTranslateEnabled, NO);

  // Open some webpage.
  [ChromeEarlGrey loadURL:URL];
  // Check that no language has been detected.
  GREYAssert(_language_detection_details.get() == nullptr,
             @"A language has been detected");
}

#pragma mark - Utility methods

// Waits until a language has been detected and checks the language details.
- (void)assertLanguageDetails:
    (const translate::LanguageDetectionDetails&)expectedDetails {
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 2.0,
                 ^{
                   return _language_detection_details.get() != nullptr;
                 }),
             @"Language not detected");

  translate::LanguageDetectionDetails details =
      *_language_detection_details.get();
  _language_detection_details.reset();

  NSString* contentLanguageError =
      [NSString stringWithFormat:@"Wrong content-language: %s (expected %s)",
                                 details.content_language.c_str(),
                                 expectedDetails.content_language.c_str()];
  GREYAssert(expectedDetails.content_language == details.content_language,
             contentLanguageError);

  NSString* htmlRootLanguageError =
      [NSString stringWithFormat:@"Wrong html root language: %s (expected %s)",
                                 details.html_root_language.c_str(),
                                 expectedDetails.html_root_language.c_str()];
  GREYAssert(expectedDetails.html_root_language == details.html_root_language,
             htmlRootLanguageError);

  NSString* adoptedLanguageError =
      [NSString stringWithFormat:@"Wrong adopted language: %s (expected %s)",
                                 details.adopted_language.c_str(),
                                 expectedDetails.adopted_language.c_str()];
  GREYAssert(expectedDetails.adopted_language == details.adopted_language,
             adoptedLanguageError);
}

@end
