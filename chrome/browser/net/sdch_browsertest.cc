// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// End-to-end SDCH tests.  Uses the embedded test server to return SDCH
// results

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "net/base/sdch_manager.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "sdch/open-vcdiff/src/google/vcencoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

namespace {

typedef std::vector<net::test_server::HttpRequest> RequestVector;
typedef std::map<std::string, std::string> HttpRequestHeaderMap;

// Credit Alfred, Lord Tennyson
static const char kSampleData[] = "<html><body><pre>"
    "There lies the port; the vessel puffs her sail:\n"
    "There gloom the dark, broad seas. My mariners,\n"
    "Souls that have toil'd, and wrought, and thought with me—\n"
    "That ever with a frolic welcome took\n"
    "The thunder and the sunshine, and opposed\n"
    "Free hearts, free foreheads—you and I are old;\n"
    "Old age hath yet his honour and his toil;\n"
    "Death closes all: but something ere the end,\n"
    "Some work of noble note, may yet be done,\n"
    "Not unbecoming men that strove with Gods.\n"
    "The lights begin to twinkle from the rocks:\n"
    "The long day wanes: the slow moon climbs: the deep\n"
    "Moans round with many voices. Come, my friends,\n"
    "'T is not too late to seek a newer world.\n"
    "Push off, and sitting well in order smite\n"
    "The sounding furrows; for my purpose holds\n"
    "To sail beyond the sunset, and the baths\n"
    "Of all the western stars, until I die.\n"
    "It may be that the gulfs will wash us down:\n"
    "It may be we shall touch the Happy Isles,\n"
    "And see the great Achilles, whom we knew.\n"
    "Tho' much is taken, much abides; and tho'\n"
    "We are not now that strength which in old days\n"
    "Moved earth and heaven, that which we are, we are;\n"
    "One equal temper of heroic hearts,\n"
    "Made weak by time and fate, but strong in will\n"
    "To strive, to seek, to find, and not to yield.\n"
    "</pre></body></html>";

// Random selection of lines from above, to allow some encoding, but
// not a trivial encoding.
static const char kDictionaryContents[] =
    "The thunder and the sunshine, and opposed\n"
    "To sail beyond the sunset, and the baths\n"
    "Of all the western stars, until I die.\n"
    "Made weak by time and fate, but strong in will\n"
    "Moans round with many voices. Come, my friends,\n"
    "The lights begin to twinkle from the rocks:";

static const char kDictionaryURLPath[] = "/dict";
static const char kDataURLPath[] = "/data";

// Scans in a case-insensitive way for |header| in |map|,
// returning true if found and setting |*value| to the value
// of that header.  Does not handle multiple instances of the same
// header.
bool GetRequestHeader(const HttpRequestHeaderMap& map,
                      const char* header,
                      std::string* value) {
  for (HttpRequestHeaderMap::const_iterator it = map.begin();
       it != map.end(); ++it) {
    if (!base::strcasecmp(it->first.c_str(), header)) {
      *value = it->second;
      return true;
    }
  }
  return false;
}

// Do a URL-safe base64 encoding.  See the SDCH spec "Dictionary Identifier"
// section, and RFC 3548 section 4.
void SafeBase64Encode(const std::string& input_value, std::string* output) {
  DCHECK(output);
  base::Base64Encode(input_value, output);
  std::replace(output->begin(), output->end(), '+', '-');
  std::replace(output->begin(), output->end(), '/', '_');
}

// Class that bundles responses for an EmbeddedTestServer().
// Dictionary is at <domain>/dict, data at <domain>/data.
// The data is sent SDCH encoded if that's allowed by protoocol.
class SdchResponseHandler {
 public:
  // Do initial preparation so that SDCH requests can be handled.
  explicit SdchResponseHandler(std::string domain)
      : cache_sdch_response_(false),
        weak_ptr_factory_(this) {
    // Dictionary
    sdch_dictionary_contents_ = "Domain: ";
    sdch_dictionary_contents_ += domain;
    sdch_dictionary_contents_ += "\n\n";
    sdch_dictionary_contents_ += kDictionaryContents;

    // Dictionary hash for client and server.
    char binary_hash[32];
    crypto::SHA256HashString(sdch_dictionary_contents_, binary_hash,
                             sizeof(binary_hash));
    SafeBase64Encode(std::string(&binary_hash[0], 6), &dictionary_client_hash_);
    SafeBase64Encode(std::string(&binary_hash[6], 6), &dictionary_server_hash_);

    // Encoded response.
    open_vcdiff::HashedDictionary vcdiff_dictionary(
        kDictionaryContents, strlen(kDictionaryContents));
    bool result = vcdiff_dictionary.Init();
    DCHECK(result);
    open_vcdiff::VCDiffStreamingEncoder encoder(&vcdiff_dictionary, 0, false);
    encoded_data_ = dictionary_server_hash_;
    encoded_data_ += '\0';
    result = encoder.StartEncoding(&encoded_data_);
    DCHECK(result);
    result = encoder.EncodeChunk(
        kSampleData, strlen(kSampleData), &encoded_data_);
    DCHECK(result);
    result = encoder.FinishEncoding(&encoded_data_);
    DCHECK(result);
  }

  static bool ClientIsAdvertisingSdchEncoding(const HttpRequestHeaderMap& map) {
    std::string value;
    if (!GetRequestHeader(map, "accept-encoding", &value))
      return false;
    base::StringTokenizer tokenizer(value, " ,");
    while (tokenizer.GetNext()) {
      if (base::strcasecmp(tokenizer.token().c_str(), "sdch"))
        return true;
    }
    return false;
  }

  bool ShouldRespondWithSdchEncoding(const HttpRequestHeaderMap& map) {
    std::string value;
    if (!GetRequestHeader(map, "avail-dictionary", &value))
      return false;
    return value == dictionary_client_hash_;
  }

  scoped_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    request_vector_.push_back(request);

    scoped_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    if (request.relative_url == kDataURLPath) {
      if (ShouldRespondWithSdchEncoding(request.headers)) {
        // Note that chrome doesn't advertise accepting SDCH encoding
        // for POSTs (because the meta-refresh hack would break a POST),
        // but that's not for the server to enforce.
        DCHECK_NE(encoded_data_, "");
        response->set_content_type("text/html");
        response->set_content(encoded_data_);
        response->AddCustomHeader("Content-Encoding", "sdch");
        // We allow tests to set caching on the sdch response,
        // so that we can force an encoded response with no
        // dictionary.
        if (cache_sdch_response_)
          response->AddCustomHeader("Cache-Control", "max-age=3600");
        else
          response->AddCustomHeader("Cache-Control", "no-store");
      } else {
        response->set_content_type("text/plain");
        response->set_content(kSampleData);
        if (ClientIsAdvertisingSdchEncoding(request.headers))
          response->AddCustomHeader("Get-Dictionary", kDictionaryURLPath);
        // We never cache the plain data response, to make it
        // easy to refresh after we get the dictionary.
        response->AddCustomHeader("Cache-Control", "no-store");
      }
    } else {
      DCHECK_EQ(request.relative_url, kDictionaryURLPath);
      DCHECK_NE(sdch_dictionary_contents_, "");
      response->set_content_type("application/x-sdch-dictionary");
      response->set_content(sdch_dictionary_contents_);
    }
    std::vector<base::Closure> callbacks;
    callbacks.swap(callback_vector_);
    for (std::vector<base::Closure>::iterator it = callbacks.begin();
         it != callbacks.end(); ++it) {
      it->Run();
    }
    return response.PassAs<net::test_server::HttpResponse>();
  }

  void WaitAndGetRequestVector(int num_requests,
                               base::Closure callback,
                               RequestVector* v) {
    DCHECK_LT(0, num_requests);
    if (static_cast<size_t>(num_requests) > request_vector_.size()) {
      callback_vector_.push_back(
          base::Bind(&SdchResponseHandler::WaitAndGetRequestVector,
                     weak_ptr_factory_.GetWeakPtr(), num_requests,
                     callback, v));
      return;
    }
    *v = request_vector_;
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE, callback);
  }

  void set_cache_sdch_response(bool cache_sdch_response) {
    cache_sdch_response_ = cache_sdch_response;
  }

 private:
  bool cache_sdch_response_;
  std::string encoded_data_;
  std::string sdch_dictionary_contents_;
  std::string dictionary_client_hash_;
  std::string dictionary_server_hash_;
  RequestVector request_vector_;
  std::vector<base::Closure> callback_vector_;
  base::WeakPtrFactory<SdchResponseHandler> weak_ptr_factory_;
};

class SdchBrowserTest : public InProcessBrowserTest, net::URLFetcherDelegate {
 public:
  static const char kTestHost[];

  SdchBrowserTest()
      : response_handler_(kTestHost),
        url_request_context_getter_(NULL),
        url_fetch_complete_(false),
        waiting_(false) {}

  // Helper functions for fetching data.

  void FetchUrlDetailed(GURL url, net::URLRequestContextGetter* getter) {
    url_fetch_complete_ = false;
    fetcher_.reset(net::URLFetcher::Create(url, net::URLFetcher::GET, this));
    fetcher_->SetRequestContext(getter);
    fetcher_->Start();
    if (!url_fetch_complete_) {
      waiting_ = true;
      content::RunMessageLoop();
      waiting_ = false;
    }
    CHECK(url_fetch_complete_);
  }

  void FetchUrl(GURL url) {
    FetchUrlDetailed(url, url_request_context_getter_);
  }

  const net::URLRequestStatus& FetcherStatus() const {
    return fetcher_->GetStatus();
  }

  int FetcherResponseCode() const {
    return (fetcher_->GetStatus().status() == net::URLRequestStatus::SUCCESS ?
            fetcher_->GetResponseCode() : 0);
  }

  const net::HttpResponseHeaders* FetcherResponseHeaders() const {
    return (fetcher_->GetStatus().status() == net::URLRequestStatus::SUCCESS ?
            fetcher_->GetResponseHeaders() : NULL);
  }

  std::string FetcherResponseContents() const {
    std::string contents;
    if (fetcher_->GetStatus().status() == net::URLRequestStatus::SUCCESS)
      CHECK(fetcher_->GetResponseAsString(&contents));
    return contents;
  }

  // Get the data from the server.  Return value is success/failure of the
  // data operation, |*sdch_encoding_used| indicates whether or not the
  // data was retrieved with sdch encoding.
  // This is done through FetchUrl(), so the various helper functions
  // will have valid status if it returns successfully.
  bool GetDataDetailed(net::URLRequestContextGetter* getter,
                       bool* sdch_encoding_used) {
    FetchUrlDetailed(
        GURL(base::StringPrintf(
            "http://%s:%d%s", kTestHost, test_server_port(), kDataURLPath)),
        getter);
    EXPECT_EQ(net::URLRequestStatus::SUCCESS, FetcherStatus().status())
        << "Error code is " << FetcherStatus().error();
    EXPECT_EQ(200, FetcherResponseCode());
    EXPECT_EQ(kSampleData, FetcherResponseContents());

    if (net::URLRequestStatus::SUCCESS != FetcherStatus().status() ||
        200 != FetcherResponseCode()) {
      *sdch_encoding_used = false;
      return false;
    }

    *sdch_encoding_used =
        FetcherResponseHeaders()->HasHeaderValue("Content-Encoding", "sdch");

    if (FetcherResponseContents() != kSampleData)
      return false;

    return true;
  }

  bool GetData(bool* sdch_encoding_used) {
    return GetDataDetailed(
        url_request_context_getter_, sdch_encoding_used);
  }

  // Client information and control.

  int GetNumberOfDictionaryFetches(Profile* profile) {
    int fetches = -1;
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&SdchBrowserTest::GetNumberOfDictionaryFetchesOnIOThread,
                   base::Unretained(profile->GetRequestContext()),
                   &fetches),
        run_loop.QuitClosure());
    run_loop.Run();
    DCHECK_NE(-1, fetches);
    return fetches;
  }

  void BrowsingDataRemoveAndWait(int remove_mask) {
    BrowsingDataRemover* remover = BrowsingDataRemover::CreateForPeriod(
        browser()->profile(), BrowsingDataRemover::LAST_HOUR);
    BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
    completion_observer.BlockUntilCompletion();
  }

  // Something of a cheat; nuke the dictionaries off the SdchManager without
  // touching the cache (which browsing data remover would do).
  void NukeSdchDictionaries() {
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&SdchBrowserTest::NukeSdchDictionariesOnIOThread,
                   url_request_context_getter_),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Create a second browser based on a second profile to work within
  // multi-profile.
  bool SetupSecondBrowser() {
    base::FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    if (!second_profile_data_dir_.CreateUniqueTempDirUnderPath(user_data_dir))
      return false;

    second_profile_ = g_browser_process->profile_manager()->GetProfile(
        second_profile_data_dir_.path());
    if (!second_profile_) return false;

    second_browser_ = new Browser(Browser::CreateParams(
        second_profile_, browser()->host_desktop_type()));
    if (!second_browser_) return false;

    chrome::AddSelectedTabWithURL(second_browser_,
                                  GURL(url::kAboutBlankURL),
                                  content::PAGE_TRANSITION_AUTO_TOPLEVEL);
    content::WaitForLoadStop(
        second_browser_->tab_strip_model()->GetActiveWebContents());
    second_browser_->window()->Show();

    return true;
  }

  Browser* second_browser() { return second_browser_; }

  // Server information and control.

  void WaitAndGetTestVector(int num_requests, RequestVector* result) {
    base::RunLoop run_loop;
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&SdchResponseHandler::WaitAndGetRequestVector,
                   base::Unretained(&response_handler_),
                   num_requests,
                   run_loop.QuitClosure(),
                   result));
    run_loop.Run();
  }

  int test_server_port() { return test_server_.port(); }

  void SetSdchCacheability(bool cache_sdch_response) {
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&SdchResponseHandler::set_cache_sdch_response,
                   base::Unretained(&response_handler_),
                   cache_sdch_response),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Helper function for common test pattern.
  //
  // This function gets the data, confirms that the initial sending of the
  // data included a dictionary advertisement, that that advertisement
  // resulted in queueing a dictionary fetch, forces that fetch to
  // go through, and confirms that a follow-on data load uses SDCH
  // encoding.  Returns true if the entire sequence of events occurred.
  bool ForceSdchDictionaryLoad(Browser* browser) {
    bool sdch_encoding_used = true;
    bool data_gotten = GetDataDetailed(
        browser->profile()->GetRequestContext(), &sdch_encoding_used);
    EXPECT_TRUE(data_gotten);
    if (!data_gotten) return false;
    EXPECT_FALSE(sdch_encoding_used);

    // Confirm that we were told to get the dictionary
    const net::HttpResponseHeaders* headers = FetcherResponseHeaders();
    std::string value;
    bool have_dict_header =
        headers->EnumerateHeader(NULL, "Get-Dictionary", &value);
    EXPECT_TRUE(have_dict_header);
    if (!have_dict_header) return false;

    // If the above didn't result in a dictionary fetch being queued, the
    // rest of the test will time out.  Avoid that.
    int num_fetches = GetNumberOfDictionaryFetches(browser->profile());
    EXPECT_EQ(1, num_fetches);
    if (1 != num_fetches) return false;

    // Wait until the dictionary fetch actually happens.
    RequestVector request_vector;
    WaitAndGetTestVector(2, &request_vector);
    EXPECT_EQ(request_vector[1].relative_url, kDictionaryURLPath);
    if (request_vector[1].relative_url != kDictionaryURLPath) return false;

    // Do a round trip to the server ignoring the encoding, presuming
    // that if we've gotten data to this thread, the dictionary's made
    // it into the SdchManager.
    data_gotten = GetDataDetailed(
        browser->profile()->GetRequestContext(), &sdch_encoding_used);
    EXPECT_TRUE(data_gotten);
    if (!data_gotten) return false;

    // Now data fetches should be SDCH encoded.
    sdch_encoding_used = false;
    data_gotten = GetDataDetailed(
        browser->profile()->GetRequestContext(), &sdch_encoding_used);
    EXPECT_TRUE(data_gotten);
    EXPECT_TRUE(sdch_encoding_used);

    if (!data_gotten || !sdch_encoding_used) return false;

    // Confirm the request vector looks at this point as expected.
    WaitAndGetTestVector(4, &request_vector);
    EXPECT_EQ(4u, request_vector.size());
    EXPECT_EQ(request_vector[2].relative_url, kDataURLPath);
    EXPECT_EQ(request_vector[3].relative_url, kDataURLPath);
    return (4u == request_vector.size() &&
            request_vector[2].relative_url == kDataURLPath &&
            request_vector[3].relative_url == kDataURLPath);
  }

 private:
  static void NukeSdchDictionariesOnIOThread(
      net::URLRequestContextGetter* context_getter) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    net::SdchManager* sdch_manager =
        context_getter->GetURLRequestContext()->sdch_manager();
    DCHECK(sdch_manager);
    sdch_manager->ClearData();
  }

  static void GetNumberOfDictionaryFetchesOnIOThread(
      net::URLRequestContextGetter* url_request_context_getter,
      int* result) {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    net::SdchManager* sdch_manager =
        url_request_context_getter->GetURLRequestContext()->sdch_manager();
    DCHECK(sdch_manager);
    *result = sdch_manager->GetFetchesCountForTesting();
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP " + std::string(kTestHost) + " 127.0.0.1");
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    test_server_.RegisterRequestHandler(
        base::Bind(&SdchResponseHandler::HandleRequest,
                   base::Unretained(&response_handler_)));
    CHECK(test_server_.InitializeAndWaitUntilReady());
    url_request_context_getter_ = browser()->profile()->GetRequestContext();
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    CHECK(test_server_.ShutdownAndWaitUntilComplete());
  }

  // URLFetcherDelegate
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    url_fetch_complete_ = true;
    if (waiting_)
      base::MessageLoopForUI::current()->Quit();
  }

  SdchResponseHandler response_handler_;
  net::test_server::EmbeddedTestServer test_server_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<net::URLFetcher> fetcher_;
  bool url_fetch_complete_;
  bool waiting_;
  base::ScopedTempDir second_profile_data_dir_;
  Profile* second_profile_;
  Browser* second_browser_;
};

const char SdchBrowserTest::kTestHost[] = "our.test.host.com";

// Confirm that after getting a dictionary, calling the browsing
// data remover renders it unusable.  Also (in calling
// ForceSdchDictionaryLoad()) servers as a smoke test for SDCH.
IN_PROC_BROWSER_TEST_F(SdchBrowserTest, BrowsingDataRemover) {
  ASSERT_TRUE(ForceSdchDictionaryLoad(browser()));

  // Confirm browsing data remover without removing the cache leaves
  // SDCH alone.
  BrowsingDataRemoveAndWait(BrowsingDataRemover::REMOVE_ALL &
                            ~BrowsingDataRemover::REMOVE_CACHE);
  bool sdch_encoding_used = false;
  ASSERT_TRUE(GetData(&sdch_encoding_used));
  EXPECT_TRUE(sdch_encoding_used);

  // Confirm browsing data remover removing the cache clears SDCH state.
  BrowsingDataRemoveAndWait(BrowsingDataRemover::REMOVE_CACHE);
  sdch_encoding_used = false;
  ASSERT_TRUE(GetData(&sdch_encoding_used));
  EXPECT_FALSE(sdch_encoding_used);
}

// Confirm dictionaries not visible in other profiles.
IN_PROC_BROWSER_TEST_F(SdchBrowserTest, Isolation) {
  ASSERT_TRUE(ForceSdchDictionaryLoad(browser()));
  ASSERT_TRUE(SetupSecondBrowser());

  // Data fetches from incognito or separate profiles should not be SDCH
  // encoded.
  bool sdch_encoding_used = true;
  Browser* incognito_browser = CreateIncognitoBrowser();
  EXPECT_TRUE(GetDataDetailed(
      incognito_browser->profile()->GetRequestContext(),
      &sdch_encoding_used));
  EXPECT_FALSE(sdch_encoding_used);

  sdch_encoding_used = true;
  EXPECT_TRUE(GetDataDetailed(
      second_browser()->profile()->GetRequestContext(), &sdch_encoding_used));
  EXPECT_FALSE(sdch_encoding_used);
}

// Confirm a dictionary loaded in incognito isn't visible in the main profile.
IN_PROC_BROWSER_TEST_F(SdchBrowserTest, ReverseIsolation) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(ForceSdchDictionaryLoad(incognito_browser));

  // Data fetches on main browser should not be SDCH encoded.
  bool sdch_encoding_used = true;
  ASSERT_TRUE(GetData(&sdch_encoding_used));
  EXPECT_FALSE(sdch_encoding_used);
}

}  // namespace
