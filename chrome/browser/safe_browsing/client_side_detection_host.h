// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/browser_feature_extractor.h"
#include "chrome/browser/safe_browsing/database_manager.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/resource_request_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

namespace safe_browsing {
class ClientPhishingRequest;
class ClientSideDetectionService;

// This class is used to receive the IPC from the renderer which
// notifies the browser that a URL was classified as phishing.  This
// class relays this information to the client-side detection service
// class which sends a ping to a server to validate the verdict.
// TODO(noelutz): move all client-side detection IPCs to this class.
class ClientSideDetectionHost : public content::WebContentsObserver,
                                public content::NotificationObserver,
                                public SafeBrowsingUIManager::Observer {
 public:
  // The caller keeps ownership of the tab object and is responsible for
  // ensuring that it stays valid until WebContentsDestroyed is called.
  static ClientSideDetectionHost* Create(content::WebContents* tab);
  virtual ~ClientSideDetectionHost();

  // From content::WebContentsObserver.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // From content::WebContentsObserver.  If we navigate away we cancel all
  // pending callbacks that could show an interstitial, and check to see whether
  // we should classify the new URL.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Called when the SafeBrowsingService found a hit with one of the
  // SafeBrowsing lists.  This method is called on the UI thread.
  virtual void OnSafeBrowsingHit(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;

  // Called when the SafeBrowsingService finds a match on the SB lists.
  // Called on the UI thread. Called even if the resource is whitelisted.
  virtual void OnSafeBrowsingMatch(
      const SafeBrowsingUIManager::UnsafeResource& resource) OVERRIDE;

  virtual scoped_refptr<SafeBrowsingDatabaseManager> database_manager();

  // Returns whether the current page contains a malware or phishing safe
  // browsing match.
  bool DidPageReceiveSafeBrowsingMatch() const;

 protected:
  explicit ClientSideDetectionHost(content::WebContents* tab);

  // From content::WebContentsObserver.
  virtual void WebContentsDestroyed() OVERRIDE;

  // Used for testing.
  void set_safe_browsing_managers(
      SafeBrowsingUIManager* ui_manager,
      SafeBrowsingDatabaseManager* database_manager);

 private:
  friend class ClientSideDetectionHostTest;
  class ShouldClassifyUrlRequest;
  friend class ShouldClassifyUrlRequest;

  // These methods are called when pre-classification checks are done for
  // the phishing and malware clasifiers.
  void OnPhishingPreClassificationDone(bool should_classify);
  void OnMalwarePreClassificationDone(bool should_classify);

  // Verdict is an encoded ClientPhishingRequest protocol message.
  void OnPhishingDetectionDone(const std::string& verdict);

  // Callback that is called when the server ping back is
  // done. Display an interstitial if |is_phishing| is true.
  // Otherwise, we do nothing.  Called in UI thread.
  void MaybeShowPhishingWarning(GURL phishing_url, bool is_phishing);

  // Callback that is called when the malware IP server ping back is
  // done. Display an interstitial if |is_malware| is true.
  // Otherwise, we do nothing.  Called in UI thread.
  void MaybeShowMalwareWarning(GURL original_url, GURL malware_url,
                               bool is_malware);

  // Callback that is called when the browser feature extractor is done.
  // This method is responsible for deleting the request object.  Called on
  // the UI thread.
  void FeatureExtractionDone(bool success,
                             scoped_ptr<ClientPhishingRequest> request);

  // Start malware classification once the onload handler was called and
  // malware pre-classification checks are done and passed.
  void MaybeStartMalwareFeatureExtraction();

  // Function to be called when the browser malware feature extractor is done.
  // Called on the UI thread.
  void MalwareFeatureExtractionDone(
      bool success, scoped_ptr<ClientMalwareRequest> request);

  // Update the entries in browse_info_->ips map.
  void UpdateIPUrlMap(const std::string& ip,
                      const std::string& url,
                      const std::string& method,
                      const std::string& referrer,
                      const content::ResourceType resource_type);

  // From NotificationObserver.  Called when a notification comes in.  This
  // method is called in the UI thread.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Inherited from WebContentsObserver.  This is called once the page is
  // done loading.
  virtual void DidStopLoading(content::RenderViewHost* rvh) OVERRIDE;

  // Returns true if the user has seen a regular SafeBrowsing
  // interstitial for the current page.  This is only true if the user has
  // actually clicked through the warning.  This method is called on the UI
  // thread.
  bool DidShowSBInterstitial() const;

  // Used for testing.  This function does not take ownership of the service
  // class.
  void set_client_side_detection_service(ClientSideDetectionService* service);

  // This pointer may be NULL if client-side phishing detection is disabled.
  ClientSideDetectionService* csd_service_;
  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  // Keep a handle to the latest classification request so that we can cancel
  // it if necessary.
  scoped_refptr<ShouldClassifyUrlRequest> classification_request_;
  // Browser-side feature extractor.
  scoped_ptr<BrowserFeatureExtractor> feature_extractor_;
  // Keeps some info about the current page visit while the renderer
  // classification is going on.  Since we cancel classification on
  // every page load we can simply keep this data around as a member
  // variable.  This information will be passed on to the feature extractor.
  scoped_ptr<BrowseInfo> browse_info_;
  // Redirect chain that leads to the first page of the current host. We keep
  // track of this for browse_info_.
  std::vector<GURL> cur_host_redirects_;
  // Current host, used to help determine cur_host_redirects_.
  std::string cur_host_;
  // Handles registering notifications with the NotificationService.
  content::NotificationRegistrar registrar_;

  // Max number of ips we save for each browse
  static const size_t kMaxIPsPerBrowse;
  // Max number of urls we report for each malware IP.
  static const size_t kMaxUrlsPerIP;

  bool should_extract_malware_features_;
  bool should_classify_for_malware_;
  bool pageload_complete_;

  base::WeakPtrFactory<ClientSideDetectionHost> weak_factory_;

  // Unique page ID of the most recent unsafe site that was loaded in this tab
  // as well as the UnsafeResource.
  int unsafe_unique_page_id_;
  scoped_ptr<SafeBrowsingUIManager::UnsafeResource> unsafe_resource_;

  DISALLOW_COPY_AND_ASSIGN(ClientSideDetectionHost);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLIENT_SIDE_DETECTION_HOST_H_
