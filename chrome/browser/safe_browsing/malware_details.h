// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_MALWARE_DETAILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_MALWARE_DETAILS_H_

// A class that encapsulates the detailed malware reports sent when
// users opt-in to do so from the malware warning page.

// An instance of this class is generated when a malware warning page
// is shown (SafeBrowsingBlockingPage).

#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/safe_browsing/report.pb.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/completion_callback.h"

namespace net {
class URLRequestContextGetter;
}

class MalwareDetailsCacheCollector;
class MalwareDetailsRedirectsCollector;
class MalwareDetailsFactory;
class Profile;
struct SafeBrowsingHostMsg_MalwareDOMDetails_Node;

namespace safe_browsing {
// Maps a URL to its Resource.
typedef base::hash_map<
  std::string,
  linked_ptr<safe_browsing::ClientMalwareReportRequest::Resource> > ResourceMap;
}

class MalwareDetails : public base::RefCountedThreadSafe<MalwareDetails>,
                       public content::WebContentsObserver {
 public:
  typedef SafeBrowsingUIManager::UnsafeResource UnsafeResource;

  // Constructs a new MalwareDetails instance, using the factory.
  static MalwareDetails* NewMalwareDetails(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instanciate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(MalwareDetailsFactory* factory) {
    factory_ = factory;
  }

  // The SafeBrowsingBlockingPage calls this from the IO thread when
  // the user is leaving the blocking page and has opted-in to sending
  // the report. We start the redirection urls collection from history service
  // in UI thread; then do cache collection back in IO thread.
  // When we are done, we send the report.
  void FinishCollection();

  void OnCacheCollectionReady();

  void OnRedirectionCollectionReady();

  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 protected:
  friend class MalwareDetailsFactoryImpl;

  MalwareDetails(SafeBrowsingUIManager* ui_manager,
                 content::WebContents* web_contents,
                 const UnsafeResource& resource);

  virtual ~MalwareDetails();

  // Called on the IO thread with the DOM details.
  virtual void AddDOMDetails(
      const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params);

  Profile* profile_;

  // The report protocol buffer.
  scoped_ptr<safe_browsing::ClientMalwareReportRequest> report_;

  // Used to get a pointer to the HTTP cache.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

 private:
  friend class base::RefCountedThreadSafe<MalwareDetails>;

  // Starts the collection of the report.
  void StartCollection();

  // Whether the url is "public" so we can add it to the report.
  bool IsPublicUrl(const GURL& url) const;

  // Finds an existing Resource for the given url, or creates a new
  // one if not found, and adds it to |resources_|. Returns the
  // found/created resource.
  safe_browsing::ClientMalwareReportRequest::Resource* FindOrCreateResource(
      const GURL& url);

  // Adds a Resource to resources_ with the given parent-child
  // relationship. |parent| and |tagname| can be empty, |children| can be NULL.
  void AddUrl(const GURL& url,
              const GURL& parent,
              const std::string& tagname,
              const std::vector<GURL>* children);

  // Message handler.
  void OnReceivedMalwareDOMDetails(
      const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params);

  void AddRedirectUrlList(const std::vector<GURL>& urls);

  scoped_refptr<SafeBrowsingUIManager> ui_manager_;

  const UnsafeResource resource_;

  // For every Url we collect we create a Resource message. We keep
  // them in a map so we can avoid duplicates.
  safe_browsing::ResourceMap resources_;

  // Result from the cache extractor.
  bool cache_result_;

  // The factory used to instanciate SafeBrowsingBlockingPage objects.
  // Usefull for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static MalwareDetailsFactory* factory_;

  // Used to collect details from the HTTP Cache.
  scoped_refptr<MalwareDetailsCacheCollector> cache_collector_;

  // Used to collect redirect urls from the history service
  scoped_refptr<MalwareDetailsRedirectsCollector> redirects_collector_;

  FRIEND_TEST_ALL_PREFIXES(MalwareDetailsTest, MalwareDOMDetails);
  FRIEND_TEST_ALL_PREFIXES(MalwareDetailsTest, HTTPCache);
  FRIEND_TEST_ALL_PREFIXES(MalwareDetailsTest, HTTPCacheNoEntries);
  FRIEND_TEST_ALL_PREFIXES(MalwareDetailsTest, HistoryServiceUrls);

  DISALLOW_COPY_AND_ASSIGN(MalwareDetails);
};

// Factory for creating MalwareDetails.  Useful for tests.
class MalwareDetailsFactory {
 public:
  virtual ~MalwareDetailsFactory() { }

  virtual MalwareDetails* CreateMalwareDetails(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) = 0;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_MALWARE_DETAILS_H_
