// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/test/app/navigation_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/third_party/material_components_ios/src/components/ProgressView/src/MaterialProgressView.h"
#include "ios/web/public/test/http_server/html_response_provider.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Text to display in form page.
const char kFormPageText[] = "Form testing page";

// Text to display in infinite loading page.
const char kPageText[] = "Navigation testing page";

// Identifier of form to submit on form page.
const char kFormID[] = "testform";

// URL string for a form page.
const char kFormURL[] = "http://form";

// URL string for an infinite pending page.
const char kInfinitePendingPageURL[] = "http://infinite";

// URL string for a simple page containing |kPageText|.
const char kSimplePageURL[] = "http://simplepage";

// Matcher for progress view.
id<GREYMatcher> ProgressView() {
  return grey_kindOfClass([MDCProgressView class]);
}

// Matcher for the progress view with |progress|.
id<GREYMatcher> ProgressViewWithProgress(CGFloat progress) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    MDCProgressView* progressView = base::mac::ObjCCast<MDCProgressView>(view);
    return progressView && progressView.progress == progress;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:@"progress view with progress:%f",
                                              progress]];
  };

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Response provider that serves the page which never finishes loading.
// TODO(crbug.com/638674): Evaluate if this can move to shared code.
class InfinitePendingResponseProvider : public HtmlResponseProvider {
 public:
  explicit InfinitePendingResponseProvider(const GURL& url) : url_(url) {}
  ~InfinitePendingResponseProvider() override {}

  // HtmlResponseProvider overrides:
  bool CanHandleRequest(const Request& request) override {
    return request.url == url_ ||
           request.url == GetInfinitePendingResponseUrl();
  }
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    if (request.url == url_) {
      *headers = GetDefaultResponseHeaders();
      *response_body =
          base::StringPrintf("<p>%s</p><img src='%s'/>", kPageText,
                             GetInfinitePendingResponseUrl().spec().c_str());
    } else if (request.url == GetInfinitePendingResponseUrl()) {
      base::PlatformThread::Sleep(base::TimeDelta::FromDays(1));
    } else {
      NOTREACHED();
    }
  }

 private:
  // Returns a url for which this response provider will never reply.
  GURL GetInfinitePendingResponseUrl() const {
    GURL::Replacements replacements;
    replacements.SetPathStr("resource");
    return url_.GetOrigin().ReplaceComponents(replacements);
  }

  // Main page URL that never finish loading.
  GURL url_;
};

}  // namespace

// Tests webpage loading progress indicator.
@interface ProgressIndicatorTestCase : ChromeTestCase
@end

@implementation ProgressIndicatorTestCase

// Returns an HTML string for a form with the submission action set to
// |submitURL|.
- (std::string)formPageHTMLWithFormSubmitURL:(GURL)submitURL {
  return base::StringPrintf(
      "<p>%s</p><form id='%s' method='post' action='%s'>"
      "<input type='submit'></form>",
      kFormPageText, kFormID, submitURL.spec().c_str());
}

// Returns an HTML string for a form with a submit event that returns false.
- (std::string)formPageHTMLWithSuppressedSubmitEvent {
  return base::StringPrintf(
      "<p>%s</p><form id='%s' method='post' onsubmit='return false'>"
      "<input type='submit'></form>",
      kFormPageText, kFormID);
}

// Tests that the progress indicator is shown and has expected progress value
// for a simple two item page, and the toolbar is visible.
- (void)testProgressIndicatorShown {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // Load a page which never finishes loading.
  const GURL infinitePendingURL =
      web::test::HttpServer::MakeUrl(kInfinitePendingPageURL);
  web::test::SetUpHttpServer(
      base::MakeUnique<InfinitePendingResponseProvider>(infinitePendingURL));

  // The page being loaded never completes, so call the LoadUrl helper that
  // does not wait for the page to complete loading.
  chrome_test_util::LoadUrl(infinitePendingURL);

  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebViewContainingText:kPageText];

  // Verify progress view visible and halfway progress.
  [[EarlGrey selectElementWithMatcher:ProgressViewWithProgress(0.5)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the progress indicator is shown and has expected progress value
// after a form is submitted, and the toolbar is visible.
- (void)testProgressIndicatorShownOnFormSubmit {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // TODO(crbug.com/747442): Re-enable this test once the bug is fixed.
  if (base::ios::IsRunningOnIOS11OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iOS 11.");
  }

  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  const GURL infinitePendingURL =
      web::test::HttpServer::MakeUrl(kInfinitePendingPageURL);

  // Create a page with a form to test.
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithFormSubmitURL:infinitePendingURL];
  web::test::SetUpSimpleHttpServer(responses);

  // Add responseProvider for page that never finishes loading.
  web::test::AddResponseProvider(
      base::MakeUnique<InfinitePendingResponseProvider>(infinitePendingURL));

  // Load form first.
  [ChromeEarlGrey loadURL:formURL];
  [ChromeEarlGrey waitForWebViewContainingText:kFormPageText];

  chrome_test_util::SubmitWebViewFormWithId(kFormID);

  // Wait until the page is half loaded.
  [ChromeEarlGrey waitForWebViewContainingText:kPageText];

  // Verify progress view visible and halfway progress.
  [[EarlGrey selectElementWithMatcher:ProgressViewWithProgress(0.5)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [ChromeEarlGreyUI waitForToolbarVisible:YES];
}

// Tests that the progress indicator disappears after form has been submitted.
- (void)testProgressIndicatorDisappearsAfterFormSubmit {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // TODO(crbug.com/747442): Re-enable this test once the bug is fixed.
  if (base::ios::IsRunningOnIOS11OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Disabled on iOS 11.");
  }

  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  const GURL simplePageURL = web::test::HttpServer::MakeUrl(kSimplePageURL);

  // Create a page with a form to test.
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithFormSubmitURL:simplePageURL];
  responses[simplePageURL] = kPageText;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:formURL];

  [ChromeEarlGrey waitForWebViewContainingText:kFormPageText];

  chrome_test_util::SubmitWebViewFormWithId(kFormID);

  // Verify the new page has been loaded.
  [ChromeEarlGrey waitForWebViewContainingText:kPageText];

  // Verify progress view is not visible.
  [[EarlGrey selectElementWithMatcher:ProgressView()]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the progress indicator disappears after form post attempt with a
// submit event that returns false.
- (void)testProgressIndicatorDisappearsAfterSuppressedFormPost {
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Skipped for iPad (no progress view in tablet)");
  }

  // Create a page with a form to test.
  const GURL formURL = web::test::HttpServer::MakeUrl(kFormURL);
  std::map<GURL, std::string> responses;
  responses[formURL] = [self formPageHTMLWithSuppressedSubmitEvent];
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:formURL];

  // Verify the form page has been loaded.
  [ChromeEarlGrey waitForWebViewContainingText:kFormPageText];

  chrome_test_util::SubmitWebViewFormWithId(kFormID);

  // Verify progress view is not visible.
  [[EarlGrey selectElementWithMatcher:grey_kindOfClass([MDCProgressView class])]
      assertWithMatcher:grey_notVisible()];
}

@end
