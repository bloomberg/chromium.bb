// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetails class.

#include "components/safe_browsing/browser/threat_details.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details_cache.h"
#include "components/safe_browsing/browser/threat_details_history.h"
#include "components/safe_browsing/common/safebrowsing_messages.h"
#include "components/safe_browsing_db/hit_report.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::RenderFrameHost;
using content::WebContents;

// Keep in sync with KMaxNodes in components/safe_browsing/renderer/
// threat_dom_details.cc
static const uint32_t kMaxDomNodes = 500;

namespace safe_browsing {

// static
ThreatDetailsFactory* ThreatDetails::factory_ = NULL;

namespace {

typedef std::unordered_set<std::string> StringSet;
// A set of HTTPS headers that are allowed to be collected. Contains both
// request and response headers. All entries in this list should be lower-case
// to support case-insensitive comparison.
struct WhitelistedHttpsHeadersTraits
    : base::internal::DestructorAtExitLazyInstanceTraits<StringSet> {
  static StringSet* New(void* instance) {
    StringSet* headers =
        base::internal::DestructorAtExitLazyInstanceTraits<StringSet>::New(
            instance);
    headers->insert({"google-creative-id", "google-lineitem-id", "referer",
                     "content-type", "content-length", "date", "server",
                     "cache-control", "pragma", "expires"});
    return headers;
  }
};
base::LazyInstance<StringSet, WhitelistedHttpsHeadersTraits>
    g_https_headers_whitelist = LAZY_INSTANCE_INITIALIZER;

// Helper function that converts SBThreatType to
// ClientSafeBrowsingReportRequest::ReportType.
ClientSafeBrowsingReportRequest::ReportType GetReportTypeFromSBThreatType(
    SBThreatType threat_type) {
  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_PHISHING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_MALWARE;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return ClientSafeBrowsingReportRequest::URL_UNWANTED;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE;
    case SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_PASSWORD_PROTECTION_PHISHING;
    default:  // Gated by SafeBrowsingBlockingPage::ShouldReportThreatDetails.
      NOTREACHED() << "We should not send report for threat type "
                   << threat_type;
      return ClientSafeBrowsingReportRequest::UNKNOWN;
  }
}

// Clears the specified HTTPS resource of any sensitive data, only retaining
// data that is whitelisted for collection.
void ClearHttpsResource(ClientSafeBrowsingReportRequest::Resource* resource) {
  // Make a copy of the original resource to retain all data.
  ClientSafeBrowsingReportRequest::Resource orig_resource(*resource);

  // Clear the request headers and copy over any whitelisted ones.
  resource->clear_request();
  for (int i = 0; i < orig_resource.request().headers_size(); ++i) {
    ClientSafeBrowsingReportRequest::HTTPHeader* orig_header =
        orig_resource.mutable_request()->mutable_headers(i);
    if (g_https_headers_whitelist.Get().count(
            base::ToLowerASCII(orig_header->name())) > 0) {
      resource->mutable_request()->add_headers()->Swap(orig_header);
    }
  }
  // Also copy some other request fields.
  resource->mutable_request()->mutable_bodydigest()->swap(
      *orig_resource.mutable_request()->mutable_bodydigest());
  resource->mutable_request()->set_bodylength(
      orig_resource.request().bodylength());

  // ...repeat for response headers.
  resource->clear_response();
  for (int i = 0; i < orig_resource.response().headers_size(); ++i) {
    ClientSafeBrowsingReportRequest::HTTPHeader* orig_header =
        orig_resource.mutable_response()->mutable_headers(i);
    if (g_https_headers_whitelist.Get().count(
            base::ToLowerASCII(orig_header->name())) > 0) {
      resource->mutable_response()->add_headers()->Swap(orig_header);
    }
  }
  // Also copy some other response fields.
  resource->mutable_response()->mutable_bodydigest()->swap(
      *orig_resource.mutable_response()->mutable_bodydigest());
  resource->mutable_response()->set_bodylength(
      orig_resource.response().bodylength());
  resource->mutable_response()->mutable_remote_ip()->swap(
      *orig_resource.mutable_response()->mutable_remote_ip());
}

std::string GetElementKey(const int frame_tree_node_id,
                          const int element_node_id) {
  return base::StringPrintf("%d-%d", frame_tree_node_id, element_node_id);
}

using CSBRR = safe_browsing::ClientSafeBrowsingReportRequest;
CSBRR::SafeBrowsingUrlApiType GetUrlApiTypeForThreatSource(
    safe_browsing::ThreatSource source) {
  switch (source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      return CSBRR::FLYWHEEL;
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      return CSBRR::PVER3_NATIVE;
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return CSBRR::PVER4_NATIVE;
    case safe_browsing::ThreatSource::REMOTE:
      return CSBRR::ANDROID_SAFETYNET;
    case safe_browsing::ThreatSource::UNKNOWN:
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
    case safe_browsing::ThreatSource::PASSWORD_PROTECTION_SERVICE:
      break;
  }
  return CSBRR::SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED;
}
}  // namespace

// The default ThreatDetailsFactory.  Global, made a singleton so we
// don't leak it.
class ThreatDetailsFactoryImpl : public ThreatDetailsFactory {
 public:
  ThreatDetails* CreateThreatDetails(
      BaseUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      net::URLRequestContextGetter* request_context_getter,
      history::HistoryService* history_service) override {
    return new ThreatDetails(ui_manager, web_contents, unsafe_resource,
                             request_context_getter, history_service);
  }

 private:
  friend struct base::LazyInstanceTraitsBase<ThreatDetailsFactoryImpl>;

  ThreatDetailsFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(ThreatDetailsFactoryImpl);
};

static base::LazyInstance<ThreatDetailsFactoryImpl>::DestructorAtExit
    g_threat_details_factory_impl = LAZY_INSTANCE_INITIALIZER;

// Create a ThreatDetails for the given tab.
/* static */
ThreatDetails* ThreatDetails::NewThreatDetails(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service) {
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_threat_details_factory_impl.Pointer();
  return factory_->CreateThreatDetails(ui_manager, web_contents, resource,
                                       request_context_getter, history_service);
}

// Create a ThreatDetails for the given tab. Runs in the UI thread.
ThreatDetails::ThreatDetails(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service)
    : content::WebContentsObserver(web_contents),
      request_context_getter_(request_context_getter),
      ui_manager_(ui_manager),
      resource_(resource),
      cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      ambiguous_dom_(false),
      cache_collector_(new ThreatDetailsCacheCollector) {
  redirects_collector_ = new ThreatDetailsRedirectsCollector(
      history_service ? history_service->AsWeakPtr()
                      : base::WeakPtr<history::HistoryService>());
  StartCollection();
}

ThreatDetails::ThreatDetails()
    : cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      ambiguous_dom_(false) {}

ThreatDetails::~ThreatDetails() {}

bool ThreatDetails::OnMessageReceived(const IPC::Message& message,
                                      RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ThreatDetails, message, render_frame_host)
    IPC_MESSAGE_HANDLER(SafeBrowsingHostMsg_ThreatDOMDetails,
                        OnReceivedThreatDOMDetails)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool ThreatDetails::IsReportableUrl(const GURL& url) const {
  // TODO(panayiotis): also skip internal urls.
  return url.SchemeIs("http") || url.SchemeIs("https");
}

// Looks for a Resource for the given url in resources_.  If found, it
// updates |resource|. Otherwise, it creates a new message, adds it to
// resources_ and updates |resource| to point to it.
//
ClientSafeBrowsingReportRequest::Resource* ThreatDetails::FindOrCreateResource(
    const GURL& url) {
  auto& resource = resources_[url.spec()];
  if (!resource) {
    // Create the resource for |url|.
    int id = resources_.size() - 1;
    std::unique_ptr<ClientSafeBrowsingReportRequest::Resource> new_resource(
        new ClientSafeBrowsingReportRequest::Resource());
    new_resource->set_url(url.spec());
    new_resource->set_id(id);
    resource = std::move(new_resource);
  }
  return resource.get();
}

HTMLElement* ThreatDetails::FindOrCreateElement(
    const std::string& element_key) {
  auto& element = elements_[element_key];
  if (!element) {
    // Create an entry for this element.
    int element_dom_id = elements_.size() - 1;
    std::unique_ptr<HTMLElement> new_element(new HTMLElement());
    new_element->set_id(element_dom_id);
    element = std::move(new_element);
  }
  return element.get();
}

ClientSafeBrowsingReportRequest::Resource* ThreatDetails::AddUrl(
    const GURL& url,
    const GURL& parent,
    const std::string& tagname,
    const std::vector<GURL>* children) {
  if (!url.is_valid() || !IsReportableUrl(url))
    return nullptr;

  // Find (or create) the resource for the url.
  ClientSafeBrowsingReportRequest::Resource* url_resource =
      FindOrCreateResource(url);
  if (!tagname.empty())
    url_resource->set_tag_name(tagname);
  if (!parent.is_empty() && IsReportableUrl(parent)) {
    // Add the resource for the parent.
    ClientSafeBrowsingReportRequest::Resource* parent_resource =
        FindOrCreateResource(parent);
    // Update the parent-child relation
    url_resource->set_parent_id(parent_resource->id());
  }
  if (children) {
    for (std::vector<GURL>::const_iterator it = children->begin();
         it != children->end(); ++it) {
      // TODO(lpz): Should this first check if the child URL is reportable
      // before creating the resource?
      ClientSafeBrowsingReportRequest::Resource* child_resource =
          FindOrCreateResource(*it);
      bool duplicate_child = false;
      for (auto child_id : url_resource->child_ids()) {
        if (child_id == child_resource->id()) {
          duplicate_child = true;
          break;
        }
      }
      if (!duplicate_child)
        url_resource->add_child_ids(child_resource->id());
    }
  }

  return url_resource;
}

void ThreatDetails::AddDomElement(
    const int frame_tree_node_id,
    const int element_node_id,
    const std::string& tagname,
    const int parent_element_node_id,
    const std::vector<AttributeNameValue>& attributes,
    const ClientSafeBrowsingReportRequest::Resource* resource) {
  // Create the element. It should not exist already since this function should
  // only be called once for each element.
  const std::string element_key =
      GetElementKey(frame_tree_node_id, element_node_id);
  HTMLElement* cur_element = FindOrCreateElement(element_key);

  // Set some basic metadata about the element.
  const std::string tag_name_upper = base::ToUpperASCII(tagname);
  if (!tag_name_upper.empty()) {
    cur_element->set_tag(tag_name_upper);
  }
  for (const AttributeNameValue& attribute : attributes) {
    HTMLElement::Attribute* attribute_pb = cur_element->add_attribute();
    attribute_pb->set_name(attribute.first);
    attribute_pb->set_value(attribute.second);
  }

  if (resource) {
    cur_element->set_resource_id(resource->id());
  }

  // Next we try to lookup the parent of the current element and add ourselves
  // as a child of it.
  HTMLElement* parent_element = nullptr;
  if (parent_element_node_id == 0) {
    // No parent indicates that this element is at the top of the current frame.
    // Remember that this is a top-level element of the frame with the
    // current |frame_tree_node_id|. If this element is inside an iframe, a
    // second pass will insert this element as a child of its parent iframe.
    frame_tree_id_to_children_map_[frame_tree_node_id].insert(
        cur_element->id());
  } else {
    // We have a parent ID, so this element is just a child of something inside
    // of our current frame. We can easily lookup our parent.
    const std::string& parent_key =
        GetElementKey(frame_tree_node_id, parent_element_node_id);
    if (base::ContainsKey(elements_, parent_key)) {
      parent_element = elements_[parent_key].get();
    }
  }

  // If a parent element was found, add ourselves as a child, ensuring not to
  // duplicate child IDs.
  if (parent_element) {
    bool duplicate_child = false;
    for (const int child_id : parent_element->child_ids()) {
      if (child_id == cur_element->id()) {
        duplicate_child = true;
        break;
      }
    }
    if (!duplicate_child) {
      parent_element->add_child_ids(cur_element->id());
    }
  }
}

void ThreatDetails::StartCollection() {
  DVLOG(1) << "Starting to compute threat details.";
  report_.reset(new ClientSafeBrowsingReportRequest());

  if (IsReportableUrl(resource_.url)) {
    report_->set_url(resource_.url.spec());
    report_->set_type(GetReportTypeFromSBThreatType(resource_.threat_type));
  }

  if (resource_.threat_type == SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING)
    report_->set_token(resource_.token);

  GURL referrer_url;
  NavigationEntry* nav_entry = resource_.GetNavigationEntryForResource();
  if (nav_entry) {
    GURL page_url = nav_entry->GetURL();
    if (IsReportableUrl(page_url))
      report_->set_page_url(page_url.spec());

    referrer_url = nav_entry->GetReferrer().url;
    if (IsReportableUrl(referrer_url))
      report_->set_referrer_url(referrer_url.spec());

    // Add the nodes, starting from the page url.
    AddUrl(page_url, GURL(), std::string(), NULL);
  }

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
  if (!referrer_url.is_empty())
    AddUrl(referrer_url, GURL(), std::string(), NULL);

  if (!resource_.IsMainPageLoadBlocked()) {
    // Get URLs of frames, scripts etc from the DOM.
    // OnReceivedThreatDOMDetails will be called when the renderer replies.
    // TODO(mattm): In theory, if the user proceeds through the warning DOM
    // detail collection could be started once the page loads.
    web_contents()->SendToAllFrames(
        new SafeBrowsingMsg_GetThreatDOMDetails(MSG_ROUTING_NONE));
  }
}

// When the renderer is done, this is called.
void ThreatDetails::OnReceivedThreatDOMDetails(
    content::RenderFrameHost* sender,
    const std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>& params) {
  // Lookup the FrameTreeNode ID of any child frames in the list of DOM nodes.
  const int sender_process_id = sender->GetProcess()->GetID();
  const int sender_frame_tree_node_id = sender->GetFrameTreeNodeId();
  KeyToFrameTreeIdMap child_frame_tree_map;
  for (const SafeBrowsingHostMsg_ThreatDOMDetails_Node& node : params) {
    if (node.child_frame_routing_id == 0)
      continue;

    const std::string cur_element_key =
        GetElementKey(sender_frame_tree_node_id, node.node_id);
    int child_frame_tree_node_id =
        content::RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
            sender_process_id, node.child_frame_routing_id);
    if (child_frame_tree_node_id ==
        content::RenderFrameHost::kNoFrameTreeNodeId) {
      ambiguous_dom_ = true;
    } else {
      child_frame_tree_map[cur_element_key] = child_frame_tree_node_id;
    }
  }

  // Schedule this in IO thread, so it doesn't conflict with future users
  // of our data structures (eg GetSerializedReport).
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ThreatDetails::AddDOMDetails, this, sender_frame_tree_node_id,
                 params, child_frame_tree_map));
}

void ThreatDetails::AddDOMDetails(
    const int frame_tree_node_id,
    const std::vector<SafeBrowsingHostMsg_ThreatDOMDetails_Node>& params,
    const KeyToFrameTreeIdMap& child_frame_tree_map) {
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

  // Exit early if there are no nodes to process.
  if (params.empty())
    return;

  // Copy FrameTreeNode IDs for the child frame into the combined mapping.
  iframe_key_to_frame_tree_id_map_.insert(child_frame_tree_map.begin(),
                                          child_frame_tree_map.end());

  // Add the urls from the DOM to |resources_|. The renderer could be sending
  // bogus messages, so limit the number of nodes we accept.
  // Also update |elements_| with the DOM structure.
  for (size_t i = 0; i < params.size() && i < kMaxDomNodes; ++i) {
    SafeBrowsingHostMsg_ThreatDOMDetails_Node node = params[i];
    DVLOG(1) << node.url << ", " << node.tag_name << ", " << node.parent;
    ClientSafeBrowsingReportRequest::Resource* resource = nullptr;
    if (!node.url.is_empty()) {
      resource = AddUrl(node.url, node.parent, node.tag_name, &(node.children));
    }
    // Check for a tag_name to avoid adding the summary node to the DOM.
    if (!node.tag_name.empty()) {
      AddDomElement(frame_tree_node_id, node.node_id, node.tag_name,
                    node.parent_node_id, node.attributes, resource);
    }
  }
}

// Called from the SB Service on the IO thread, after the user has
// closed the tab, or clicked proceed or goback.  Since the user needs
// to take an action, we expect this to be called after
// OnReceivedThreatDOMDetails in most cases. If not, we don't include
// the DOM data in our report.
void ThreatDetails::FinishCollection(bool did_proceed, int num_visit) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Do a second pass over the elements and update iframe elements to have
  // references to their children. Children may have been received from a
  // different renderer than the iframe element.
  for (auto& element_pair : elements_) {
    const std::string& element_key = element_pair.first;
    HTMLElement* element = element_pair.second.get();
    if (base::ContainsKey(iframe_key_to_frame_tree_id_map_, element_key)) {
      int frame_tree_id_of_iframe_renderer =
          iframe_key_to_frame_tree_id_map_[element_key];
      const std::unordered_set<int>& child_ids =
          frame_tree_id_to_children_map_[frame_tree_id_of_iframe_renderer];
      for (const int child_id : child_ids) {
        element->add_child_ids(child_id);
      }
    }
  }
  did_proceed_ = did_proceed;
  num_visits_ = num_visit;
  std::vector<GURL> urls;
  for (ResourceMap::const_iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    urls.push_back(GURL(it->first));
  }
  redirects_collector_->StartHistoryCollection(
      urls, base::Bind(&ThreatDetails::OnRedirectionCollectionReady, this));
}

void ThreatDetails::OnRedirectionCollectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  const std::vector<RedirectChain>& redirects =
      redirects_collector_->GetCollectedUrls();

  for (size_t i = 0; i < redirects.size(); ++i)
    AddRedirectUrlList(redirects[i]);

  // Call the cache collector
  cache_collector_->StartCacheCollection(
      request_context_getter_.get(), &resources_, &cache_result_,
      base::Bind(&ThreatDetails::OnCacheCollectionReady, this));
}

void ThreatDetails::AddRedirectUrlList(const std::vector<GURL>& urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (size_t i = 0; i < urls.size() - 1; ++i) {
    AddUrl(urls[i], urls[i + 1], std::string(), NULL);
  }
}

void ThreatDetails::OnCacheCollectionReady() {
  DVLOG(1) << "OnCacheCollectionReady.";
  // Add all the urls in our |resources_| maps to the |report_| protocol buffer.
  for (auto& resource_pair : resources_) {
    ClientSafeBrowsingReportRequest::Resource* pb_resource =
        report_->add_resources();
    pb_resource->Swap(resource_pair.second.get());
    const GURL url(pb_resource->url());
    if (url.SchemeIs("https")) {
      // Sanitize the HTTPS resource by clearing out private data (like cookie
      // headers).
      DVLOG(1) << "Clearing out HTTPS resource: " << pb_resource->url();
      ClearHttpsResource(pb_resource);
      // Keep id, parent_id, child_ids, and tag_name.
    }
  }
  for (auto& element_pair : elements_) {
    report_->add_dom()->Swap(element_pair.second.get());
  }
  if (!elements_.empty()) {
    // TODO(lpz): Consider including the ambiguous_dom_ bit in the report
    // itself.
    UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.ThreatReport.DomIsAmbiguous",
                          ambiguous_dom_);
  }

  report_->set_did_proceed(did_proceed_);
  // Only sets repeat_visit if num_visits_ >= 0.
  if (num_visits_ >= 0) {
    report_->set_repeat_visit(num_visits_ > 0);
  }
  report_->set_complete(cache_result_);

  report_->mutable_client_properties()->set_url_api_type(
      GetUrlApiTypeForThreatSource(resource_.threat_source));

  // Send the report, using the SafeBrowsingService.
  std::string serialized;
  if (!report_->SerializeToString(&serialized)) {
    DLOG(ERROR) << "Unable to serialize the threat report.";
    return;
  }
  ui_manager_->SendSerializedThreatDetails(serialized);
}

}  // namespace safe_browsing
