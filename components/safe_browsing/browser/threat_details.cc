// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetails class.

#include "components/safe_browsing/browser/threat_details.h"

#include <stddef.h>
#include <stdint.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/threat_details_cache.h"
#include "components/safe_browsing/browser/threat_details_history.h"
#include "components/safe_browsing/db/hit_report.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/service_manager/public/cpp/interface_provider.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::RenderFrameHost;
using content::WebContents;

// Keep in sync with KMaxNodes in components/safe_browsing/renderer/
// threat_dom_details.cc
static const uint32_t kMaxDomNodes = 500;

namespace safe_browsing {

// static
ThreatDetailsFactory* ThreatDetails::factory_ = nullptr;

namespace {

// An element ID indicating that an HTML Element has no parent.
const int kElementIdNoParent = -1;

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
    case SB_THREAT_TYPE_AD_SAMPLE:
      return ClientSafeBrowsingReportRequest::AD_SAMPLE;
    case SB_THREAT_TYPE_PASSWORD_REUSE:
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

void TrimElements(const std::set<int> target_ids,
                  ElementMap* elements,
                  ResourceMap* resources) {
  if (target_ids.empty()) {
    elements->clear();
    resources->clear();
    return;
  }

  // First, scan over the elements and create a list ordered by element ID as
  // well as a reverse mapping from element ID to its parent ID.
  std::vector<HTMLElement*> elements_by_id(elements->size());

  // The parent vector is initialized with |kElementIdNoParent| so we can
  // identify elements that have no parent.
  std::vector<int> element_id_to_parent_id(elements->size(),
                                           kElementIdNoParent);
  for (const auto& element_pair : *elements) {
    HTMLElement* element = element_pair.second.get();
    elements_by_id[element->id()] = element;

    for (int child_id : element->child_ids()) {
      element_id_to_parent_id[child_id] = element->id();
    }
  }

  // Create a similar map for resources, ordered by resource ID.
  std::vector<std::string> resource_id_to_url(resources->size());
  for (const auto& resource_pair : *resources) {
    const std::string& url = resource_pair.first;
    ClientSafeBrowsingReportRequest::Resource* resource =
        resource_pair.second.get();
    resource_id_to_url[resource->id()] = url;
  }

  // Take a second pass and determine which element IDs to keep. We want to keep
  // the immediate parent, the siblings, and the children of the target ids.
  // By keeping the parent of the target and all of its children, this covers
  // the target's siblings as well.
  std::vector<int> ids_to_keep;
  // Keep track of ids that were kept to avoid duplication. We still need the
  // vector above for handling the children where it is used like a queue.
  std::unordered_set<int> kept_ids;
  for (int target_id : target_ids) {
    const int parent_id = element_id_to_parent_id[target_id];
    if (parent_id == kElementIdNoParent) {
      // If one of the target elements has no parent then we skip trimming the
      // report further. Since we collect all siblings of this element, it will
      // effectively span the whole report, so no trimming necessary.
      return;
    }

    // Otherwise, insert the parent ID into the list of ids to keep. This will
    // capture the parent and siblings of the target element, as well as each of
    // their children.
    if (kept_ids.count(parent_id) == 0) {
      ids_to_keep.push_back(parent_id);
      kept_ids.insert(parent_id);
    }
  }

  // Walk through |ids_to_keep| and append the children of each of element to
  // |ids_to_keep|. This is effectively a breadth-first traversal of the tree.
  // The list will stop growing when we reach the leaf nodes that have no more
  // children.
  for (size_t index = 0; index < ids_to_keep.size(); ++index) {
    int cur_element_id = ids_to_keep[index];
    const HTMLElement& element = *(elements_by_id[cur_element_id]);
    for (int child_id : element.child_ids()) {
      ids_to_keep.push_back(child_id);
    }
  }
  // Sort the list for easier lookup below.
  std::sort(ids_to_keep.begin(), ids_to_keep.end());

  // Now we know which elements we want to keep, scan through |elements| and
  // erase anything that we aren't keeping. If an erased element refers to a
  // resource then remove it from |resources| as well.
  for (auto element_iter = elements->begin();
       element_iter != elements->end();) {
    const HTMLElement& element = *element_iter->second;

    // Delete any elements that we do not want to keep.
    if (std::find(ids_to_keep.begin(), ids_to_keep.end(), element.id()) ==
        ids_to_keep.end()) {
      if (element.has_resource_id()) {
        const std::string& resource_url =
            resource_id_to_url[element.resource_id()];
        resources->erase(resource_url);
      }
      element_iter = elements->erase(element_iter);
    } else {
      ++element_iter;
    }
  }
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
      history::HistoryService* history_service,
      bool trim_to_ad_tags,
      ThreatDetailsDoneCallback done_callback) override {
    return new ThreatDetails(ui_manager, web_contents, unsafe_resource,
                             request_context_getter, history_service,
                             trim_to_ad_tags, done_callback);
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
    history::HistoryService* history_service,
    bool trim_to_ad_tags,
    ThreatDetailsDoneCallback done_callback) {
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_threat_details_factory_impl.Pointer();
  return factory_->CreateThreatDetails(ui_manager, web_contents, resource,
                                       request_context_getter, history_service,
                                       trim_to_ad_tags, done_callback);
}

// Create a ThreatDetails for the given tab. Runs in the UI thread.
ThreatDetails::ThreatDetails(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const UnsafeResource& resource,
    net::URLRequestContextGetter* request_context_getter,
    history::HistoryService* history_service,
    bool trim_to_ad_tags,
    ThreatDetailsDoneCallback done_callback)
    : content::WebContentsObserver(web_contents),
      request_context_getter_(request_context_getter),
      ui_manager_(ui_manager),
      resource_(resource),
      cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      ambiguous_dom_(false),
      trim_to_ad_tags_(trim_to_ad_tags),
      cache_collector_(new ThreatDetailsCacheCollector),
      done_callback_(done_callback),
      all_done_expected_(false),
      is_all_done_(false) {
  redirects_collector_ = new ThreatDetailsRedirectsCollector(
      history_service ? history_service->AsWeakPtr()
                      : base::WeakPtr<history::HistoryService>());
  StartCollection();
}

// TODO(lpz): Consider making this constructor delegate to the parameterized one
// above.
ThreatDetails::ThreatDetails()
    : cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      ambiguous_dom_(false),
      trim_to_ad_tags_(false),
      done_callback_(nullptr),
      all_done_expected_(false),
      is_all_done_(false) {}

ThreatDetails::~ThreatDetails() {
  DCHECK(all_done_expected_ == is_all_done_);
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
    const std::vector<mojom::AttributeNameValuePtr> attributes,
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
  for (const mojom::AttributeNameValuePtr& attribute : attributes) {
    HTMLElement::Attribute* attribute_pb = cur_element->add_attribute();
    attribute_pb->set_name(std::move(attribute->name));
    attribute_pb->set_value(std::move(attribute->value));

    // Remember which the IDs of elements that represent ads so we can trim the
    // report down to just those parts later.
    if (trim_to_ad_tags_ && attribute_pb->name() == "data-google-query-id") {
      trimmed_dom_element_ids_.insert(cur_element->id());
    }
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
    AddUrl(page_url, GURL(), std::string(), nullptr);
  }

  // Add the resource_url and its original url, if non-empty and different.
  if (!resource_.original_url.is_empty() &&
      resource_.url != resource_.original_url) {
    // Add original_url, as the parent of resource_url.
    AddUrl(resource_.original_url, GURL(), std::string(), nullptr);
    AddUrl(resource_.url, resource_.original_url, std::string(), nullptr);
  } else {
    AddUrl(resource_.url, GURL(), std::string(), nullptr);
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
    AddUrl(resource_.redirect_urls[i], parent_url, std::string(), nullptr);
    parent_url = resource_.redirect_urls[i];
  }

  // Add the referrer url.
  if (!referrer_url.is_empty())
    AddUrl(referrer_url, GURL(), std::string(), nullptr);

  if (!resource_.IsMainPageLoadBlocked()) {
    // Get URLs of frames, scripts etc from the DOM.
    // OnReceivedThreatDOMDetails will be called when the renderer replies.
    // TODO(mattm): In theory, if the user proceeds through the warning DOM
    // detail collection could be started once the page loads.
    web_contents()->ForEachFrame(
        base::BindRepeating(&ThreatDetails::RequestThreatDOMDetails, this));
  }
}

void ThreatDetails::RequestThreatDOMDetails(content::RenderFrameHost* frame) {
  safe_browsing::mojom::ThreatReporterPtr threat_reporter;
  frame->GetRemoteInterfaces()->GetInterface(&threat_reporter);
  safe_browsing::mojom::ThreatReporter* raw_threat_report =
      threat_reporter.get();
  raw_threat_report->GetThreatDOMDetails(
      base::BindOnce(&ThreatDetails::OnReceivedThreatDOMDetails, this,
                     base::Passed(&threat_reporter), frame));
}

// When the renderer is done, this is called.
void ThreatDetails::OnReceivedThreatDOMDetails(
    mojom::ThreatReporterPtr threat_reporter,
    content::RenderFrameHost* sender,
    std::vector<mojom::ThreatDOMDetailsNodePtr> params) {
  // Lookup the FrameTreeNode ID of any child frames in the list of DOM nodes.
  const int sender_process_id = sender->GetProcess()->GetID();
  const int sender_frame_tree_node_id = sender->GetFrameTreeNodeId();
  KeyToFrameTreeIdMap child_frame_tree_map;
  for (const mojom::ThreatDOMDetailsNodePtr& node : params) {
    if (node->child_frame_routing_id == 0)
      continue;

    const std::string cur_element_key =
        GetElementKey(sender_frame_tree_node_id, node->node_id);
    int child_frame_tree_node_id =
        content::RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
            sender_process_id, node->child_frame_routing_id);
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
      base::BindOnce(&ThreatDetails::AddDOMDetails, this,
                     sender_frame_tree_node_id, std::move(params),
                     child_frame_tree_map));
}

void ThreatDetails::AddDOMDetails(
    const int frame_tree_node_id,
    std::vector<mojom::ThreatDOMDetailsNodePtr> params,
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
    mojom::ThreatDOMDetailsNode& node = *params[i];
    DVLOG(1) << node.url << ", " << node.tag_name << ", " << node.parent;
    ClientSafeBrowsingReportRequest::Resource* resource = nullptr;
    if (!node.url.is_empty()) {
      resource = AddUrl(node.url, node.parent, node.tag_name, &(node.children));
    }
    // Check for a tag_name to avoid adding the summary node to the DOM.
    if (!node.tag_name.empty()) {
      AddDomElement(frame_tree_node_id, node.node_id, node.tag_name,
                    node.parent_node_id, std::move(node.attributes), resource);
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

  all_done_expected_ = true;

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

  if (trim_to_ad_tags_) {
    TrimElements(trimmed_dom_element_ids_, &elements_, &resources_);
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
    AddUrl(urls[i], urls[i + 1], std::string(), nullptr);
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
    AllDone();
    return;
  }

  // For measuring performance impact of ad sampling reports, we may want to
  // do all the heavy lifting of creating the report but not actually send it.
  if (report_->type() == ClientSafeBrowsingReportRequest::AD_SAMPLE &&
      base::FeatureList::IsEnabled(kAdSamplerCollectButDontSendFeature)) {
    AllDone();
    return;
  }

  BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&WebUIInfoSingleton::AddToReportsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(report_)));
  ui_manager_->SendSerializedThreatDetails(serialized);

  AllDone();
}

void ThreatDetails::AllDone() {
  is_all_done_ = true;
  BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(done_callback_, base::Unretained(web_contents())));
}
}  // namespace safe_browsing
