// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_string_value_serializer.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver_proc.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
using testing::HasSubstr;

namespace {

// Records a history of all hostnames for which resolving has been requested,
// and immediately fails the resolution requests themselves.
class HostResolutionRequestRecorder : public net::HostResolverProc {
 public:
  HostResolutionRequestRecorder()
      : HostResolverProc(NULL),
        is_waiting_for_hostname_(false) {
  }

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&HostResolutionRequestRecorder::AddToHistory,
                   base::Unretained(this),
                   host));
    return net::ERR_NAME_NOT_RESOLVED;
  }

  bool HasHostBeenRequested(const std::string& hostname) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return std::find(requested_hostnames_.begin(),
                     requested_hostnames_.end(),
                     hostname) != requested_hostnames_.end();
  }

  void WaitUntilHostHasBeenRequested(const std::string& hostname) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(!is_waiting_for_hostname_);
    if (HasHostBeenRequested(hostname))
      return;
    waiting_for_hostname_ = hostname;
    is_waiting_for_hostname_ = true;
    content::RunMessageLoop();
  }

 private:
  virtual ~HostResolutionRequestRecorder() {}

  void AddToHistory(const std::string& hostname) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    requested_hostnames_.push_back(hostname);
    if (is_waiting_for_hostname_ && waiting_for_hostname_ == hostname) {
      is_waiting_for_hostname_ = false;
      waiting_for_hostname_.clear();
      MessageLoop::current()->Quit();
    }
  }

  // The hostname which WaitUntilHostHasBeenRequested is currently waiting for
  // to be requested.
  std::string waiting_for_hostname_;

  // Whether WaitUntilHostHasBeenRequested is waiting for a hostname to be
  // requested and thus is running a nested message loop.
  bool is_waiting_for_hostname_;

  // A list of hostnames for which resolution has already been requested. Only
  // to be accessed from the UI thread.
  std::vector<std::string> requested_hostnames_;

  DISALLOW_COPY_AND_ASSIGN(HostResolutionRequestRecorder);
};

}  // namespace

namespace chrome_browser_net {

class PredictorBrowserTest : public InProcessBrowserTest {
 public:
  PredictorBrowserTest()
      : startup_url_("http://host1:1"),
        referring_url_("http://host2:1"),
        target_url_("http://host3:1"),
        host_resolution_request_recorder_(new HostResolutionRequestRecorder) {
  }

 protected:
  virtual void SetUp() OVERRIDE {
    net::ScopedDefaultHostResolverProc scoped_host_resolver_proc(
        host_resolution_request_recorder_);
    InProcessBrowserTest::SetUp();
  }

  void LearnAboutInitialNavigation(const GURL& url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&Predictor::LearnAboutInitialNavigation,
                                       base::Unretained(predictor),
                                       url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void LearnFromNavigation(const GURL& referring_url, const GURL& target_url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&Predictor::LearnFromNavigation,
                                       base::Unretained(predictor),
                                       referring_url,
                                       target_url));
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  void PrepareFrameSubresources(const GURL& url) {
    Predictor* predictor = browser()->profile()->GetNetworkPredictor();
    predictor->PredictFrameSubresources(url);
  }

  void GetListFromPrefsAsString(const char* list_path,
                                std::string* value_as_string) const {
    PrefService* prefs = browser()->profile()->GetPrefs();
    const base::ListValue* list_value = prefs->GetList(list_path);
    JSONStringValueSerializer serializer(value_as_string);
    serializer.Serialize(*list_value);
  }

  void WaitUntilHostHasBeenRequested(const std::string& hostname) {
    host_resolution_request_recorder_->WaitUntilHostHasBeenRequested(hostname);
  }

  const GURL startup_url_;
  const GURL referring_url_;
  const GURL target_url_;

 private:
  scoped_refptr<HostResolutionRequestRecorder>
      host_resolution_request_recorder_;
};

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, PRE_ShutdownStartupCycle) {
  // Prepare state that will be serialized on this shut-down and read on next
  // start-up.
  LearnAboutInitialNavigation(startup_url_);
  LearnFromNavigation(referring_url_, target_url_);
}

IN_PROC_BROWSER_TEST_F(PredictorBrowserTest, ShutdownStartupCycle) {
  // Make sure that the Preferences file is actually wiped of all DNS prefetch
  // related data after start-up.
  std::string cleared_startup_list;
  std::string cleared_referral_list;
  GetListFromPrefsAsString(prefs::kDnsPrefetchingStartupList,
                           &cleared_startup_list);
  GetListFromPrefsAsString(prefs::kDnsPrefetchingHostReferralList,
                           &cleared_referral_list);

  EXPECT_THAT(cleared_startup_list, Not(HasSubstr(startup_url_.host())));
  EXPECT_THAT(cleared_referral_list, Not(HasSubstr(referring_url_.host())));
  EXPECT_THAT(cleared_referral_list, Not(HasSubstr(target_url_.host())));

  // But also make sure this data has been first loaded into the Predictor, by
  // inspecting that the Predictor starts making the expected hostname requests.
  PrepareFrameSubresources(referring_url_);
  WaitUntilHostHasBeenRequested(startup_url_.host());
  WaitUntilHostHasBeenRequested(target_url_.host());
}

}  // namespace chrome_browser_net

