// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/serialized_navigation_entry.h"

#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "sync/protocol/session_specifics.pb.h"
#include "sync/util/time.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"

using content::NavigationEntry;

namespace sessions {

const char kSearchTermsKey[] = "search_terms";

SerializedNavigationEntry::SerializedNavigationEntry()
    : index_(-1),
      unique_id_(0),
      transition_type_(content::PAGE_TRANSITION_TYPED),
      has_post_data_(false),
      post_id_(-1),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_restored_(false),
      blocked_state_(STATE_INVALID) {}

SerializedNavigationEntry::~SerializedNavigationEntry() {}

// static
SerializedNavigationEntry SerializedNavigationEntry::FromNavigationEntry(
    int index,
    const NavigationEntry& entry) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = entry.GetUniqueID();
  navigation.referrer_ = entry.GetReferrer();
  navigation.virtual_url_ = entry.GetVirtualURL();
  navigation.title_ = entry.GetTitle();
  navigation.page_state_ = entry.GetPageState();
  navigation.transition_type_ = entry.GetTransitionType();
  navigation.has_post_data_ = entry.GetHasPostData();
  navigation.post_id_ = entry.GetPostID();
  navigation.original_request_url_ = entry.GetOriginalRequestURL();
  navigation.is_overriding_user_agent_ = entry.GetIsOverridingUserAgent();
  navigation.timestamp_ = entry.GetTimestamp();
  navigation.is_restored_ = entry.IsRestored();
  // If you want to navigate a named frame in Chrome, you will first need to
  // add support for persisting it. It is currently only used for layout tests.
  CHECK(entry.GetFrameToNavigate().empty());
  entry.GetExtraData(kSearchTermsKey, &navigation.search_terms_);
  if (entry.GetFavicon().valid)
    navigation.favicon_url_ = entry.GetFavicon().url;
  navigation.http_status_code_ = entry.GetHttpStatusCode();
  navigation.redirect_chain_ = entry.GetRedirectChain();

  return navigation;
}

SerializedNavigationEntry SerializedNavigationEntry::FromSyncData(
    int index,
    const sync_pb::TabNavigation& sync_data) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = sync_data.unique_id();
  navigation.referrer_ = content::Referrer(
      GURL(sync_data.referrer()),
      static_cast<blink::WebReferrerPolicy>(sync_data.referrer_policy()));
  navigation.virtual_url_ = GURL(sync_data.virtual_url());
  navigation.title_ = base::UTF8ToUTF16(sync_data.title());
  navigation.page_state_ =
      content::PageState::CreateFromEncodedData(sync_data.state());

  uint32 transition = 0;
  if (sync_data.has_page_transition()) {
    switch (sync_data.page_transition()) {
      case sync_pb::SyncEnums_PageTransition_LINK:
        transition = content::PAGE_TRANSITION_LINK;
        break;
      case sync_pb::SyncEnums_PageTransition_TYPED:
        transition = content::PAGE_TRANSITION_TYPED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK:
        transition = content::PAGE_TRANSITION_AUTO_BOOKMARK;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME:
        transition = content::PAGE_TRANSITION_AUTO_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME:
        transition = content::PAGE_TRANSITION_MANUAL_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_GENERATED:
        transition = content::PAGE_TRANSITION_GENERATED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL:
        transition = content::PAGE_TRANSITION_AUTO_TOPLEVEL;
        break;
      case sync_pb::SyncEnums_PageTransition_FORM_SUBMIT:
        transition = content::PAGE_TRANSITION_FORM_SUBMIT;
        break;
      case sync_pb::SyncEnums_PageTransition_RELOAD:
        transition = content::PAGE_TRANSITION_RELOAD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD:
        transition = content::PAGE_TRANSITION_KEYWORD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED:
        transition =
            content::PAGE_TRANSITION_KEYWORD_GENERATED;
        break;
      default:
        transition = content::PAGE_TRANSITION_LINK;
        break;
    }
  }

  if  (sync_data.has_redirect_type()) {
    switch (sync_data.redirect_type()) {
      case sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT:
        transition |= content::PAGE_TRANSITION_CLIENT_REDIRECT;
        break;
      case sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT:
        transition |= content::PAGE_TRANSITION_SERVER_REDIRECT;
        break;
    }
  }
  if (sync_data.navigation_forward_back())
      transition |= content::PAGE_TRANSITION_FORWARD_BACK;
  if (sync_data.navigation_from_address_bar())
      transition |= content::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  if (sync_data.navigation_home_page())
      transition |= content::PAGE_TRANSITION_HOME_PAGE;
  if (sync_data.navigation_chain_start())
      transition |= content::PAGE_TRANSITION_CHAIN_START;
  if (sync_data.navigation_chain_end())
      transition |= content::PAGE_TRANSITION_CHAIN_END;

  navigation.transition_type_ =
      static_cast<content::PageTransition>(transition);

  navigation.timestamp_ = base::Time();
  navigation.search_terms_ = base::UTF8ToUTF16(sync_data.search_terms());
  if (sync_data.has_favicon_url())
    navigation.favicon_url_ = GURL(sync_data.favicon_url());

  navigation.http_status_code_ = sync_data.http_status_code();

  navigation.Sanitize();

  navigation.is_restored_ = true;

  return navigation;
}

namespace {

// Helper used by SerializedNavigationEntry::WriteToPickle(). It writes |str| to
// |pickle|, if and only if |str| fits within (|max_bytes| -
// |*bytes_written|).  |bytes_written| is incremented to reflect the
// data written.
//
// TODO(akalin): Unify this with the same function in
// base_session_service.cc.
void WriteStringToPickle(Pickle* pickle,
                         int* bytes_written,
                         int max_bytes,
                         const std::string& str) {
  int num_bytes = str.size() * sizeof(char);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString(str);
  } else {
    pickle->WriteString(std::string());
  }
}

// base::string16 version of WriteStringToPickle.
//
// TODO(akalin): Unify this, too.
void WriteString16ToPickle(Pickle* pickle,
                           int* bytes_written,
                           int max_bytes,
                           const base::string16& str) {
  int num_bytes = str.size() * sizeof(base::char16);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString16(str);
  } else {
    pickle->WriteString16(base::string16());
  }
}

// A mask used for arbitrary boolean values needed to represent a
// NavigationEntry. Currently only contains HAS_POST_DATA.
//
// NOTE(akalin): We may want to just serialize |has_post_data_|
// directly.  Other bools (|is_overriding_user_agent_|) haven't been
// added to this mask.
enum TypeMask {
  HAS_POST_DATA = 1
};

}  // namespace

// Pickle order:
//
// index_
// virtual_url_
// title_
// page_state_
// transition_type_
//
// Added on later:
//
// type_mask (has_post_data_)
// referrer_
// original_request_url_
// is_overriding_user_agent_
// timestamp_
// search_terms_
// http_status_code_

void SerializedNavigationEntry::WriteToPickle(int max_size,
                                              Pickle* pickle) const {
  pickle->WriteInt(index_);

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_size,
                      virtual_url_.spec());

  WriteString16ToPickle(pickle, &bytes_written, max_size, title_);

  content::PageState page_state = page_state_;
  if (has_post_data_)
    page_state = page_state.RemovePasswordData();

  WriteStringToPickle(pickle, &bytes_written, max_size,
                      page_state.ToEncodedData());

  pickle->WriteInt(transition_type_);

  const int type_mask = has_post_data_ ? HAS_POST_DATA : 0;
  pickle->WriteInt(type_mask);

  WriteStringToPickle(
      pickle, &bytes_written, max_size,
      referrer_.url.is_valid() ? referrer_.url.spec() : std::string());

  pickle->WriteInt(referrer_.policy);

  // Save info required to override the user agent.
  WriteStringToPickle(
      pickle, &bytes_written, max_size,
      original_request_url_.is_valid() ?
      original_request_url_.spec() : std::string());
  pickle->WriteBool(is_overriding_user_agent_);
  pickle->WriteInt64(timestamp_.ToInternalValue());

  WriteString16ToPickle(pickle, &bytes_written, max_size, search_terms_);

  pickle->WriteInt(http_status_code_);
}

bool SerializedNavigationEntry::ReadFromPickle(PickleIterator* iterator) {
  *this = SerializedNavigationEntry();
  std::string virtual_url_spec, page_state_data;
  int transition_type_int = 0;
  if (!iterator->ReadInt(&index_) ||
      !iterator->ReadString(&virtual_url_spec) ||
      !iterator->ReadString16(&title_) ||
      !iterator->ReadString(&page_state_data) ||
      !iterator->ReadInt(&transition_type_int))
    return false;
  virtual_url_ = GURL(virtual_url_spec);
  page_state_ = content::PageState::CreateFromEncodedData(page_state_data);
  transition_type_ = static_cast<content::PageTransition>(transition_type_int);

  // type_mask did not always exist in the written stream. As such, we
  // don't fail if it can't be read.
  int type_mask = 0;
  bool has_type_mask = iterator->ReadInt(&type_mask);

  if (has_type_mask) {
    has_post_data_ = type_mask & HAS_POST_DATA;
    // the "referrer" property was added after type_mask to the written
    // stream. As such, we don't fail if it can't be read.
    std::string referrer_spec;
    if (!iterator->ReadString(&referrer_spec))
      referrer_spec = std::string();
    // The "referrer policy" property was added even later, so we fall back to
    // the default policy if the property is not present.
    int policy_int;
    blink::WebReferrerPolicy policy;
    if (iterator->ReadInt(&policy_int))
      policy = static_cast<blink::WebReferrerPolicy>(policy_int);
    else
      policy = blink::WebReferrerPolicyDefault;
    referrer_ = content::Referrer(GURL(referrer_spec), policy);

    // If the original URL can't be found, leave it empty.
    std::string original_request_url_spec;
    if (!iterator->ReadString(&original_request_url_spec))
      original_request_url_spec = std::string();
    original_request_url_ = GURL(original_request_url_spec);

    // Default to not overriding the user agent if we don't have info.
    if (!iterator->ReadBool(&is_overriding_user_agent_))
      is_overriding_user_agent_ = false;

    int64 timestamp_internal_value = 0;
    if (iterator->ReadInt64(&timestamp_internal_value)) {
      timestamp_ = base::Time::FromInternalValue(timestamp_internal_value);
    } else {
      timestamp_ = base::Time();
    }

    // If the search terms field can't be found, leave it empty.
    if (!iterator->ReadString16(&search_terms_))
      search_terms_.clear();

    if (!iterator->ReadInt(&http_status_code_))
      http_status_code_ = 0;
  }

  Sanitize();

  is_restored_ = true;

  return true;
}

scoped_ptr<NavigationEntry> SerializedNavigationEntry::ToNavigationEntry(
    int page_id,
    content::BrowserContext* browser_context) const {
  scoped_ptr<NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          virtual_url_,
          referrer_,
          // Use a transition type of reload so that we don't incorrectly
          // increase the typed count.
          content::PAGE_TRANSITION_RELOAD,
          false,
          // The extra headers are not sync'ed across sessions.
          std::string(),
          browser_context));

  entry->SetTitle(title_);
  entry->SetPageState(page_state_);
  entry->SetPageID(page_id);
  entry->SetHasPostData(has_post_data_);
  entry->SetPostID(post_id_);
  entry->SetOriginalRequestURL(original_request_url_);
  entry->SetIsOverridingUserAgent(is_overriding_user_agent_);
  entry->SetTimestamp(timestamp_);
  entry->SetExtraData(kSearchTermsKey, search_terms_);
  entry->SetHttpStatusCode(http_status_code_);
  entry->SetRedirectChain(redirect_chain_);

  // These fields should have default values.
  DCHECK_EQ(STATE_INVALID, blocked_state_);
  DCHECK_EQ(0u, content_pack_categories_.size());

  return entry.Pass();
}

// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
sync_pb::TabNavigation SerializedNavigationEntry::ToSyncData() const {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(virtual_url_.spec());
  sync_data.set_referrer(referrer_.url.spec());
  sync_data.set_referrer_policy(referrer_.policy);
  sync_data.set_title(base::UTF16ToUTF8(title_));

  // Page transition core.
  COMPILE_ASSERT(content::PAGE_TRANSITION_LAST_CORE ==
                 content::PAGE_TRANSITION_KEYWORD_GENERATED,
                 PageTransitionCoreBounds);
  switch (PageTransitionStripQualifier(transition_type_)) {
    case content::PAGE_TRANSITION_LINK:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_LINK);
      break;
    case content::PAGE_TRANSITION_TYPED:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_TYPED);
      break;
    case content::PAGE_TRANSITION_AUTO_BOOKMARK:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK);
      break;
    case content::PAGE_TRANSITION_AUTO_SUBFRAME:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_MANUAL_SUBFRAME:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME);
      break;
    case content::PAGE_TRANSITION_GENERATED:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_GENERATED);
      break;
    case content::PAGE_TRANSITION_AUTO_TOPLEVEL:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL);
      break;
    case content::PAGE_TRANSITION_FORM_SUBMIT:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_FORM_SUBMIT);
      break;
    case content::PAGE_TRANSITION_RELOAD:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_RELOAD);
      break;
    case content::PAGE_TRANSITION_KEYWORD:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD);
      break;
    case content::PAGE_TRANSITION_KEYWORD_GENERATED:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED);
      break;
    default:
      NOTREACHED();
  }

  // Page transition qualifiers.
  if (PageTransitionIsRedirect(transition_type_)) {
    if (transition_type_ & content::PAGE_TRANSITION_CLIENT_REDIRECT) {
      sync_data.set_redirect_type(
        sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);
    } else if (transition_type_ & content::PAGE_TRANSITION_SERVER_REDIRECT) {
      sync_data.set_redirect_type(
        sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
    }
  }
  sync_data.set_navigation_forward_back(
      (transition_type_ & content::PAGE_TRANSITION_FORWARD_BACK) != 0);
  sync_data.set_navigation_from_address_bar(
      (transition_type_ & content::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  sync_data.set_navigation_home_page(
      (transition_type_ & content::PAGE_TRANSITION_HOME_PAGE) != 0);
  sync_data.set_navigation_chain_start(
      (transition_type_ & content::PAGE_TRANSITION_CHAIN_START) != 0);
  sync_data.set_navigation_chain_end(
      (transition_type_ & content::PAGE_TRANSITION_CHAIN_END) != 0);

  sync_data.set_unique_id(unique_id_);
  sync_data.set_timestamp_msec(syncer::TimeToProtoTime(timestamp_));
  // The full-resolution timestamp works as a global ID.
  sync_data.set_global_id(timestamp_.ToInternalValue());

  sync_data.set_search_terms(base::UTF16ToUTF8(search_terms_));

  sync_data.set_http_status_code(http_status_code_);

  if (favicon_url_.is_valid())
    sync_data.set_favicon_url(favicon_url_.spec());

  if (blocked_state_ != STATE_INVALID) {
    sync_data.set_blocked_state(
        static_cast<sync_pb::TabNavigation_BlockedState>(blocked_state_));
  }

  for (std::set<std::string>::const_iterator it =
           content_pack_categories_.begin();
       it != content_pack_categories_.end(); ++it) {
    sync_data.add_content_pack_categories(*it);
  }

  // Copy all redirect chain entries except the last URL (which should match
  // the virtual_url).
  if (redirect_chain_.size() > 1) {  // Single entry chains have no redirection.
    size_t last_entry = redirect_chain_.size() - 1;
    for (size_t i = 0; i < last_entry; i++) {
      sync_pb::NavigationRedirect* navigation_redirect =
          sync_data.add_navigation_redirect();
      navigation_redirect->set_url(redirect_chain_[i].spec());
    }
    // If the last URL didn't match the virtual_url, record it separately.
    if (sync_data.virtual_url() != redirect_chain_[last_entry].spec()) {
      sync_data.set_last_navigation_redirect_url(
          redirect_chain_[last_entry].spec());
    }
  }

  sync_data.set_is_restored(is_restored_);

  return sync_data;
}

// static
std::vector<NavigationEntry*> SerializedNavigationEntry::ToNavigationEntries(
    const std::vector<SerializedNavigationEntry>& navigations,
    content::BrowserContext* browser_context) {
  int page_id = 0;
  std::vector<NavigationEntry*> entries;
  for (std::vector<SerializedNavigationEntry>::const_iterator
       it = navigations.begin(); it != navigations.end(); ++it) {
    entries.push_back(
        it->ToNavigationEntry(page_id, browser_context).release());
    ++page_id;
  }
  return entries;
}

void SerializedNavigationEntry::Sanitize() {
  // Store original referrer so we can later see whether it was actually
  // changed during sanitization, and we need to strip the referrer from the
  // page state as well.
  content::Referrer old_referrer = referrer_;

  if (!referrer_.url.SchemeIsHTTPOrHTTPS())
    referrer_ = content::Referrer();
  switch (referrer_.policy) {
    case blink::WebReferrerPolicyNever:
      referrer_.url = GURL();
      break;
    case blink::WebReferrerPolicyAlways:
      break;
    case blink::WebReferrerPolicyOrigin:
      referrer_.url = referrer_.url.GetWithEmptyPath();
      break;
    case blink::WebReferrerPolicyDefault:
      // Fall through.
    default:
      referrer_.policy = blink::WebReferrerPolicyDefault;
      if (referrer_.url.SchemeIsSecure() && !virtual_url_.SchemeIsSecure())
        referrer_.url = GURL();
  }

  if (referrer_.url != old_referrer.url ||
      referrer_.policy != old_referrer.policy) {
    referrer_ = content::Referrer();
    page_state_ = page_state_.RemoveReferrer();
  }
}

}  // namespace sessions
