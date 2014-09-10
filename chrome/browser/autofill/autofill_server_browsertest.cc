// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

// TODO(isherman): Similar classes are defined in a few other Autofill browser
// tests. It would be good to factor out the shared code into a helper file.
class WindowedPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Profile* profile)
      : profile_(profile),
        message_loop_runner_(new content::MessageLoopRunner){
    PersonalDataManagerFactory::GetForProfile(profile_)->AddObserver(this);
  }
  virtual ~WindowedPersonalDataManagerObserver() {}

  // Waits for the PersonalDataManager's list of profiles to be updated.
  void Wait() {
    message_loop_runner_->Run();
    PersonalDataManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  virtual void OnPersonalDataChanged() OVERRIDE {
    message_loop_runner_->Quit();
  }

 private:
  Profile* profile_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

class WindowedNetworkObserver : public net::TestURLFetcher::DelegateForTests {
 public:
  explicit WindowedNetworkObserver(const std::string& expected_upload_data)
      : factory_(new net::TestURLFetcherFactory),
        expected_upload_data_(expected_upload_data),
        message_loop_runner_(new content::MessageLoopRunner) {
    factory_->SetDelegateForTests(this);
  }
  ~WindowedNetworkObserver() {}

  // Waits for a network request with the |expected_upload_data_|.
  void Wait() {
    message_loop_runner_->Run();
    factory_.reset();
  }

  // net::TestURLFetcher::DelegateForTests:
  virtual void OnRequestStart(int fetcher_id) OVERRIDE {
    net::TestURLFetcher* fetcher = factory_->GetFetcherByID(fetcher_id);
    if (fetcher->upload_data() == expected_upload_data_)
      message_loop_runner_->Quit();

    // Not interested in any further status updates from this fetcher.
    fetcher->SetDelegateForTests(NULL);
  }
  virtual void OnChunkUpload(int fetcher_id) OVERRIDE {}
  virtual void OnRequestEnd(int fetcher_id) OVERRIDE {}

 private:
  // Mocks out network requests.
  scoped_ptr<net::TestURLFetcherFactory> factory_;

  const std::string expected_upload_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowedNetworkObserver);
};

}  // namespace

class AutofillServerTest : public InProcessBrowserTest  {
 public:
  virtual void SetUpOnMainThread() OVERRIDE {
    // Disable interactions with the Mac Keychain.
    PrefService* pref_service = browser()->profile()->GetPrefs();
    test::DisableSystemServices(pref_service);

    // Enable uploads, and load a new tab to force the AutofillDownloadManager
    // to update its cached view of the prefs.
    pref_service->SetDouble(prefs::kAutofillPositiveUploadRate, 1.0);
    pref_service->SetDouble(prefs::kAutofillNegativeUploadRate, 1.0);
    AddBlankTabAndShow(browser());
  }
};

// Regression test for http://crbug.com/177419
IN_PROC_BROWSER_TEST_F(AutofillServerTest,
                       QueryAndUploadBothIncludeFieldsWithAutocompleteOff) {
  // Seed some test Autofill profile data, as upload requests are only made when
  // there is local data available to use as a baseline.
  WindowedPersonalDataManagerObserver personal_data_observer(
      browser()->profile());
  PersonalDataManagerFactory::GetForProfile(browser()->profile())
      ->AddProfile(test::GetFullProfile());
  personal_data_observer.Wait();

  // Load the test page. Expect a query request upon loading the page.
  const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
  const char kFormHtml[] =
      "<form id='test_form'>"
      "  <input id='one'>"
      "  <input id='two' autocomplete='off'>"
      "  <input id='three'>"
      "  <input id='four' autocomplete='off'>"
      "  <input type='submit'>"
      "</form>"
      "<script>"
      "  document.onclick = function() {"
      "    document.getElementById('test_form').submit();"
      "  };"
      "</script>";
  const char kQueryRequest[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"15916856893790176210\">"
      "<field signature=\"2594484045\"/>"
      "<field signature=\"2750915947\"/>"
      "<field signature=\"3494787134\"/>"
      "<field signature=\"1236501728\"/>"
      "</form>"
      "</autofillquery>";
  WindowedNetworkObserver query_network_observer(kQueryRequest);
  ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kFormHtml));
  query_network_observer.Wait();

  // Submit the form, using a simulated mouse click because form submissions not
  // triggered by user gestures are ignored. Expect an upload request upon form
  // submission, with form fields matching those from the query request.
  const char kUploadRequest[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
      " formsignature=\"15916856893790176210\""
      " autofillused=\"false\""
      " datapresent=\"1f7e0003780000080004\">"
      "<field signature=\"2594484045\" autofilltype=\"2\"/>"
      "<field signature=\"2750915947\" autofilltype=\"2\"/>"
      "<field signature=\"3494787134\" autofilltype=\"2\"/>"
      "<field signature=\"1236501728\" autofilltype=\"2\"/>"
      "</autofillupload>";
  WindowedNetworkObserver upload_network_observer(kUploadRequest);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::SimulateMouseClick(
      web_contents, 0, blink::WebMouseEvent::ButtonLeft);
  upload_network_observer.Wait();
}

}  // namespace autofill
