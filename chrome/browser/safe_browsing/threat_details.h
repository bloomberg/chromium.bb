// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_H_

// A class that encapsulates the detailed threat reports sent when
// users opt-in to do so from the safe browsing warning page.

// An instance of this class is generated when a safe browsing warning page
// is shown (SafeBrowsingBlockingPage).

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/completion_callback.h"

namespace net {
class URLRequestContextGetter;
}

class Profile;
struct SafeBrowsingHostMsg_ThreatDOMDetails_Node;

namespace safe_browsing {

// Maps a URL to its Resource.
class ThreatDetailsCacheCollector;
class ThreatDetailsRedirectsCollector;
class ThreatDetailsFactory;

typedef base::hash_map<
    std::string,
    linked_ptr<ClientSafeBrowsingReportRequest::Resource>>
    ResourceMap;

class ThreatDetails : public base::RefCountedThreadSafe<ThreatDetails>,
                      public content::WebContentsObserver {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  // Constructs a new ThreatDetails instance, using the factory.
  static ThreatDetails* NewThreatDetails(SafeBrowsingUIManager* ui_manager,
                                         content::WebContents* web_contents,
                                         const UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instanciate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(ThreatDetailsFactory* factory) {
    factory_ = factory;
  }

  // The SafeBrowsingBlockingPage calls this from the IO thread when
  // the user is leaving the blocking page and has opted-in to sending
  // the report. We start the redirection urls collection from history service
  // in UI thread; then do cache collection back in IO thread. We also record
  // if the user did proceed with the warning page, and how many times user
  // visited this page before. When we are done, we send the report.
  void FinishCollection(bool did_proceed, int num_visits);

  void OnCacheCollectionReady();

  void OnRedirectionCollectionReady();

  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

 protected:
  friend class ThreatDetailsFactoryImpl;

  ThreatDetails(SafeBrowsingUIManager* ui_manager,
                content::WebContents* web_contents,
                const UnsafeResource& resource);

  ~ThreatDetails() override;

  // Called on the IO thread with the DOM details.
  virtual void AddDOMDetails(
      const std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>& params);

  Profile* profile_;

  // The report protocol buffer.
  scoped_ptr<ClientSafeBrowsingReportRequest> report_;

  // Used to get a pointer to the HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

 private:
  friend class base::RefCountedThreadSafe<ThreatDetails>;

  // Starts the collection of the report.
  void StartCollection();

  // Whether the url is "public" so we can add it to the report.
  bool IsReportableUrl(const GURL& url) const;

  // Finds an existing Resource for the given url, or creates a new
  // one if not found, and adds it to |resources_|. Returns the
  // found/created resource.
  ClientSafeBrowsingReportRequest::Resource* FindOrCreateResource(
      const GURL& url);

  // Adds a Resource to resources_ with the given parent-child
  // relationship. |parent| and |tagname| can be empty, |children| can be NULL.
  void AddUrl(const GURL& url,
              const GURL& parent,
              const std::string& tagname,
              const std::vector<GURL>* children);

  // Message handler.
  void OnReceivedThreatDOMDetails(
      const std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>& params);

  void AddRedirectUrlList(const std::vector<GURL>& urls);

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  const UnsafeResource resource_;

  // For every Url we collect we create a Resource message. We keep
  // them in a map so we can avoid duplicates.
  ResourceMap resources_;

  // Result from the cache extractor.
  bool cache_result_;

  // Whether user did proceed with the safe browsing blocking page or
  // not.
  bool did_proceed_;

  // How many times this user has visited this page before.
  int num_visits_;

  // The factory used to instanciate SafeBrowsingBlockingPage objects.
  // Usefull for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static ThreatDetailsFactory* factory_;

  // Used to collect details from the HTTP Cache.
  scoped_refptr<ThreatDetailsCacheCollector> cache_collector_;

  // Used to collect redirect urls from the history service
  scoped_refptr<ThreatDetailsRedirectsCollector> redirects_collector_;

  FRIEND_TEST_ALL_PREFIXES(ThreatDetailsTest, ThreatDOMDetails);
  FRIEND_TEST_ALL_PREFIXES(ThreatDetailsTest, HTTPCache);
  FRIEND_TEST_ALL_PREFIXES(ThreatDetailsTest, HTTPCacheNoEntries);
  FRIEND_TEST_ALL_PREFIXES(ThreatDetailsTest, HistoryServiceUrls);

  DISALLOW_COPY_AND_ASSIGN(ThreatDetails);
};

// Factory for creating ThreatDetails.  Useful for tests.
class ThreatDetailsFactory {
 public:
  virtual ~ThreatDetailsFactory() {}

  virtual ThreatDetails* CreateThreatDetails(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_THREAT_DETAILS_H_
