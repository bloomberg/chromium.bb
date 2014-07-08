// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the MalwareDetails class.

#include "chrome/browser/safe_browsing/malware_details.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/malware_details_cache.h"
#include "chrome/browser/safe_browsing/malware_details_history.h"
#include "chrome/browser/safe_browsing/report.pb.h"
#include "chrome/common/safe_browsing/safebrowsing_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::WebContents;
using safe_browsing::ClientMalwareReportRequest;

// Keep in sync with KMaxNodes in renderer/safe_browsing/malware_dom_details
static const uint32 kMaxDomNodes = 500;

// static
MalwareDetailsFactory* MalwareDetails::factory_ = NULL;

// The default MalwareDetailsFactory.  Global, made a singleton so we
// don't leak it.
class MalwareDetailsFactoryImpl : public MalwareDetailsFactory {
 public:
  virtual MalwareDetails* CreateMalwareDetails(
      SafeBrowsingUIManager* ui_manager,
      WebContents* web_contents,
      const SafeBrowsingUIManager::UnsafeResource& unsafe_resource) OVERRIDE {
    return new MalwareDetails(ui_manager, web_contents, unsafe_resource);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<MalwareDetailsFactoryImpl>;

  MalwareDetailsFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(MalwareDetailsFactoryImpl);
};

static base::LazyInstance<MalwareDetailsFactoryImpl>
    g_malware_details_factory_impl = LAZY_INSTANCE_INITIALIZER;

// Create a MalwareDetails for the given tab.
/* static */
MalwareDetails* MalwareDetails::NewMalwareDetails(
    SafeBrowsingUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResource& resource) {
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_malware_details_factory_impl.Pointer();
  return factory_->CreateMalwareDetails(ui_manager, web_contents, resource);
}

// Create a MalwareDetails for the given tab. Runs in the UI thread.
MalwareDetails::MalwareDetails(
    SafeBrowsingUIManager* ui_manager,
    content::WebContents* web_contents,
    const UnsafeResource& resource)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      request_context_getter_(profile_->GetRequestContext()),
      ui_manager_(ui_manager),
      resource_(resource),
      cache_result_(false),
      cache_collector_(new MalwareDetailsCacheCollector),
      redirects_collector_(
          new MalwareDetailsRedirectsCollector(profile_)) {
  StartCollection();
}

MalwareDetails::~MalwareDetails() {
}

bool MalwareDetails::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MalwareDetails, message)
    IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_MalwareDOMDetails,
                        OnReceivedMalwareDOMDetails)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool MalwareDetails::IsPublicUrl(const GURL& url) const {
  return url.SchemeIs("http");  // TODO(panayiotis): also skip internal urls.
}

// Looks for a Resource for the given url in resources_.  If found, it
// updates |resource|. Otherwise, it creates a new message, adds it to
// resources_ and updates |resource| to point to it.
ClientMalwareReportRequest::Resource* MalwareDetails::FindOrCreateResource(
    const GURL& url) {
  safe_browsing::ResourceMap::iterator it = resources_.find(url.spec());
  if (it != resources_.end())
    return it->second.get();

  // Create the resource for |url|.
  int id = resources_.size();
  linked_ptr<ClientMalwareReportRequest::Resource> new_resource(
      new ClientMalwareReportRequest::Resource());
  new_resource->set_url(url.spec());
  new_resource->set_id(id);
  resources_[url.spec()] = new_resource;
  return new_resource.get();
}

void MalwareDetails::AddUrl(const GURL& url,
                            const GURL& parent,
                            const std::string& tagname,
                            const std::vector<GURL>* children) {
  if (!url.is_valid() || !IsPublicUrl(url))
    return;

  // Find (or create) the resource for the url.
  ClientMalwareReportRequest::Resource* url_resource =
      FindOrCreateResource(url);
  if (!tagname.empty())
    url_resource->set_tag_name(tagname);
  if (!parent.is_empty() && IsPublicUrl(parent)) {
    // Add the resource for the parent.
    ClientMalwareReportRequest::Resource* parent_resource =
        FindOrCreateResource(parent);
    // Update the parent-child relation
    url_resource->set_parent_id(parent_resource->id());
  }
  if (children) {
    for (std::vector<GURL>::const_iterator it = children->begin();
         it != children->end(); ++it) {
      ClientMalwareReportRequest::Resource* child_resource =
          FindOrCreateResource(*it);
      url_resource->add_child_ids(child_resource->id());
    }
  }
}

void MalwareDetails::StartCollection() {
  DVLOG(1) << "Starting to compute malware details.";
  report_.reset(new ClientMalwareReportRequest());

  if (IsPublicUrl(resource_.url))
    report_->set_malware_url(resource_.url.spec());

  GURL page_url = web_contents()->GetURL();
  if (IsPublicUrl(page_url))
    report_->set_page_url(page_url.spec());

  GURL referrer_url;
  NavigationEntry* nav_entry = web_contents()->GetController().GetActiveEntry();
  if (nav_entry) {
    referrer_url = nav_entry->GetReferrer().url;
    if (IsPublicUrl(referrer_url)) {
      report_->set_referrer_url(referrer_url.spec());
    }
  }

  // Add the nodes, starting from the page url.
  AddUrl(page_url, GURL(), std::string(), NULL);

  // Add the resource_url and its original url, if non-empty and different.
  if (!resource_.original_url.is_empty() &&
      resource_.url != resource_.original_url) {
    // Add original_url, as the parent of resource_url.
    AddUrl(resource_.original_url, GURL(), std::string(), NULL);
    AddUrl(resource_.url, resource_.original_url, std::string(), NULL);
  } else {
    AddUrl(resource_.url, GURL(), std::string(), NULL);
  }

  // Add the redirect urls, if non-empty. The redirect urls do not include the
  // original url, but include the unsafe url which is the last one of the
  // redirect urls chain
  GURL parent_url;
  // Set the original url as the parent of the first redirect url if it's not
  // empty.
  if (!resource_.original_url.is_empty())
    parent_url = resource_.original_url;

  // Set the previous redirect url as the parent of the next one
  for (size_t i = 0; i < resource_.redirect_urls.size(); ++i) {
    AddUrl(resource_.redirect_urls[i], parent_url, std::string(), NULL);
    parent_url = resource_.redirect_urls[i];
  }

  // Add the referrer url.
  if (nav_entry && !referrer_url.is_empty())
    AddUrl(referrer_url, GURL(), std::string(), NULL);

  // Get URLs of frames, scripts etc from the DOM.
  // OnReceivedMalwareDOMDetails will be called when the renderer replies.
  content::RenderViewHost* view = web_contents()->GetRenderViewHost();
  view->Send(new SafeBrowsingMsg_GetMalwareDOMDetails(view->GetRoutingID()));
}

// When the renderer is done, this is called.
void MalwareDetails::OnReceivedMalwareDOMDetails(
    const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params) {
  // Schedule this in IO thread, so it doesn't conflict with future users
  // of our data structures (eg GetSerializedReport).
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&MalwareDetails::AddDOMDetails, this, params));
}

void MalwareDetails::AddDOMDetails(
    const std::vector<SafeBrowsingHostMsg_MalwareDOMDetails_Node>& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "Nodes from the DOM: " << params.size();

  // If we have already started getting redirects from history service,
  // don't modify state, otherwise will invalidate the iterators.
  if (redirects_collector_->HasStarted())
    return;

  // If we have already started collecting data from the HTTP cache, don't
  // modify our state.
  if (cache_collector_->HasStarted())
    return;

  // Add the urls from the DOM to |resources_|.  The renderer could be
  // sending bogus messages, so limit the number of nodes we accept.
  for (size_t i = 0; i < params.size() && i < kMaxDomNodes; ++i) {
    SafeBrowsingHostMsg_MalwareDOMDetails_Node node = params[i];
    DVLOG(1) << node.url << ", " << node.tag_name << ", " << node.parent;
    AddUrl(node.url, node.parent, node.tag_name, &(node.children));
  }
}

// Called from the SB Service on the IO thread, after the user has
// closed the tab, or clicked proceed or goback.  Since the user needs
// to take an action, we expect this to be called after
// OnReceivedMalwareDOMDetails in most cases. If not, we don't include
// the DOM data in our report.
void MalwareDetails::FinishCollection() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::vector<GURL> urls;
  for (safe_browsing::ResourceMap::const_iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    urls.push_back(GURL(it->first));
  }
  redirects_collector_->StartHistoryCollection(
      urls,
      base::Bind(&MalwareDetails::OnRedirectionCollectionReady, this));
}

void MalwareDetails::OnRedirectionCollectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const std::vector<safe_browsing::RedirectChain>& redirects =
      redirects_collector_->GetCollectedUrls();

  for (size_t i = 0; i < redirects.size(); ++i)
    AddRedirectUrlList(redirects[i]);

  // Call the cache collector
  cache_collector_->StartCacheCollection(
      request_context_getter_.get(),
      &resources_,
      &cache_result_,
      base::Bind(&MalwareDetails::OnCacheCollectionReady, this));
}

void MalwareDetails::AddRedirectUrlList(const std::vector<GURL>& urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (size_t i = 0; i < urls.size() - 1; ++i) {
    AddUrl(urls[i], urls[i + 1], std::string(), NULL);
  }
}

void MalwareDetails::OnCacheCollectionReady() {
  DVLOG(1) << "OnCacheCollectionReady.";
  // Add all the urls in our |resources_| maps to the |report_| protocol buffer.
  for (safe_browsing::ResourceMap::const_iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    ClientMalwareReportRequest::Resource* pb_resource =
        report_->add_resources();
    pb_resource->CopyFrom(*(it->second));
  }

  report_->set_complete(cache_result_);

  // Send the report, using the SafeBrowsingService.
  std::string serialized;
  if (!report_->SerializeToString(&serialized)) {
    DLOG(ERROR) << "Unable to serialize the malware report.";
    return;
  }

  ui_manager_->SendSerializedMalwareDetails(serialized);
}
