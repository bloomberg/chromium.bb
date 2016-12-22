// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The Safe Browsing service is responsible for downloading anti-phishing and
// anti-malware tables and checking urls against them.

#ifndef CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
#define CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "components/safe_browsing/base_ui_manager.h"

class GURL;

namespace content {
class WebContents;
struct PermissionReportInfo;
}  // namespace content

namespace safe_browsing {

struct HitReport;

// Construction needs to happen on the main thread.
class SafeBrowsingUIManager : public BaseSafeBrowsingUIManager {
 public:
  explicit SafeBrowsingUIManager(
      const scoped_refptr<SafeBrowsingService>& service);

  // Called to stop or shutdown operations on the io_thread. This may be called
  // multiple times during the life of the UIManager. Should be called
  // on IO thread. If shutdown is true, the manager is disabled permanently.
  void StopOnIOThread(bool shutdown) override;

  // Called on the UI thread to display an interstitial page.
  // |url| is the url of the resource that matches a safe browsing list.
  // If the request contained a chain of redirects, |url| is the last url
  // in the chain, and |original_url| is the first one (the root of the
  // chain). Otherwise, |original_url| = |url|.
  void DisplayBlockingPage(const UnsafeResource& resource) override;

  // Log the user perceived delay caused by SafeBrowsing. This delay is the time
  // delta starting from when we would have started reading data from the
  // network, and ending when the SafeBrowsing check completes indicating that
  // the current page is 'safe'.
  void LogPauseDelay(base::TimeDelta time) override;

  // Called on the IO thread by the ThreatDetails with the serialized
  // protocol buffer, so the service can send it over.
  void SendSerializedThreatDetails(const std::string& serialized) override;

  // Report hits to the unsafe contents (malware, phishing, unsafe download URL)
  // to the server. Can only be called on UI thread.  If |post_data| is
  // non-empty, the request will be sent as a POST instead of a GET.
  // Will report only for UMA || is_extended_reporting.
  void MaybeReportSafeBrowsingHit(
      const safe_browsing::HitReport& hit_report) override;

  // Report an invalid TLS/SSL certificate chain to the server. Can only
  // be called on UI thread.
  void ReportInvalidCertificateChain(const std::string& serialized_report,
                                     const base::Closure& callback);

  // Report permission action to SafeBrowsing servers. Can only be called on UI
  // thread.
  void ReportPermissionAction(const PermissionReportInfo& report_info);

  // Creates the whitelist URL set for tests that create a blocking page
  // themselves and then simulate OnBlockingPageDone(). OnBlockingPageDone()
  // expects the whitelist to exist, but the tests don't necessarily call
  // DisplayBlockingPage(), which creates it.
  static void CreateWhitelistForTesting(content::WebContents* web_contents);

 protected:
  ~SafeBrowsingUIManager() override;

  // Call protocol manager on IO thread to report hits of unsafe contents.
  void ReportSafeBrowsingHitOnIOThread(
      const safe_browsing::HitReport& hit_report) override;

 private:
  friend class SafeBrowsingUIManagerTest;
  friend class TestSafeBrowsingUIManager;

  // Sends an invalid certificate chain report over the network.
  void ReportInvalidCertificateChainOnIOThread(
      const std::string& serialized_report);

  // Report permission action to SafeBrowsing servers.
  void ReportPermissionActionOnIOThread(
      const PermissionReportInfo& report_info);

  static GURL GetMainFrameWhitelistUrlForResourceForTesting(
      const safe_browsing::SafeBrowsingUIManager::UnsafeResource& resource);

  // Safebrowsing service.
  scoped_refptr<SafeBrowsingService> sb_service_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingUIManager);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_UI_MANAGER_H_
