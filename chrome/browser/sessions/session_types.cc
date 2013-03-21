// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_types.h"

#include "base/basictypes.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/sessions/session_command.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "sync/util/time.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebReferrerPolicy.h"
#include "webkit/glue/glue_serialize.h"

using content::NavigationEntry;

// TabNavigation --------------------------------------------------------------

TabNavigation::TabNavigation()
    : index_(-1),
      unique_id_(0),
      transition_type_(content::PAGE_TRANSITION_TYPED),
      has_post_data_(false),
      post_id_(-1),
      is_overriding_user_agent_(false) {}

TabNavigation::~TabNavigation() {}

// static
TabNavigation TabNavigation::FromNavigationEntry(
    int index,
    const NavigationEntry& entry) {
  TabNavigation navigation;
  navigation.index_ = index;
  navigation.unique_id_ = entry.GetUniqueID();
  navigation.referrer_ = entry.GetReferrer();
  navigation.virtual_url_ = entry.GetVirtualURL();
  navigation.title_ = entry.GetTitle();
  navigation.content_state_ = entry.GetContentState();
  navigation.transition_type_ = entry.GetTransitionType();
  navigation.has_post_data_ = entry.GetHasPostData();
  navigation.post_id_ = entry.GetPostID();
  navigation.original_request_url_ = entry.GetOriginalRequestURL();
  navigation.is_overriding_user_agent_ = entry.GetIsOverridingUserAgent();
  navigation.timestamp_ = entry.GetTimestamp();
  // If you want to navigate a named frame in Chrome, you will first need to
  // add support for persisting it. It is currently only used for layout tests.
  CHECK(entry.GetFrameToNavigate().empty());
  navigation.search_terms_ =
      chrome::search::GetSearchTermsFromNavigationEntry(&entry);
  if (entry.GetFavicon().valid)
    navigation.favicon_url_ = entry.GetFavicon().url;

  return navigation;
}

TabNavigation TabNavigation::FromSyncData(
    int index,
    const sync_pb::TabNavigation& sync_data) {
  TabNavigation navigation;
  navigation.index_ = index;
  navigation.unique_id_ = sync_data.unique_id();
  navigation.referrer_ =
      content::Referrer(GURL(sync_data.referrer()),
                        WebKit::WebReferrerPolicyDefault);
  navigation.virtual_url_ = GURL(sync_data.virtual_url());
  navigation.title_ = UTF8ToUTF16(sync_data.title());
  navigation.content_state_ = sync_data.state();

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
  navigation.search_terms_ = UTF8ToUTF16(sync_data.search_terms());
  if (sync_data.has_favicon_url())
    navigation.favicon_url_ = GURL(sync_data.favicon_url());

  return navigation;
}

namespace {

// Helper used by TabNavigation::WriteToPickle(). It writes |str| to
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

// string16 version of WriteStringToPickle.
//
// TODO(akalin): Unify this, too.
void WriteString16ToPickle(Pickle* pickle,
                           int* bytes_written,
                           int max_bytes,
                           const string16& str) {
  int num_bytes = str.size() * sizeof(char16);
  if (*bytes_written + num_bytes < max_bytes) {
    *bytes_written += num_bytes;
    pickle->WriteString16(str);
  } else {
    pickle->WriteString16(string16());
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
// content_state_
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

void TabNavigation::WriteToPickle(Pickle* pickle) const {
  pickle->WriteInt(index_);

  // We only allow navigations up to 63k (which should be completely
  // reasonable). On the off chance we get one that is too big, try to
  // keep the url.

  // Bound the string data (which is variable length) to
  // |max_state_size bytes| bytes.
  static const size_t max_state_size =
      std::numeric_limits<SessionCommand::size_type>::max() - 1024;
  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_state_size,
                      virtual_url_.spec());

  WriteString16ToPickle(pickle, &bytes_written, max_state_size, title_);

  std::string content_state = content_state_;
  if (has_post_data_) {
    content_state =
        webkit_glue::RemovePasswordDataFromHistoryState(content_state);
  }
  WriteStringToPickle(pickle, &bytes_written, max_state_size, content_state);

  pickle->WriteInt(transition_type_);

  const int type_mask = has_post_data_ ? HAS_POST_DATA : 0;
  pickle->WriteInt(type_mask);

  WriteStringToPickle(
      pickle, &bytes_written, max_state_size,
      referrer_.url.is_valid() ? referrer_.url.spec() : std::string());

  pickle->WriteInt(referrer_.policy);

  // Save info required to override the user agent.
  WriteStringToPickle(
      pickle, &bytes_written, max_state_size,
      original_request_url_.is_valid() ?
      original_request_url_.spec() : std::string());
  pickle->WriteBool(is_overriding_user_agent_);
  pickle->WriteInt64(timestamp_.ToInternalValue());

  WriteString16ToPickle(pickle, &bytes_written, max_state_size, search_terms_);
}

bool TabNavigation::ReadFromPickle(PickleIterator* iterator) {
  *this = TabNavigation();
  std::string virtual_url_spec;
  int transition_type_int = 0;
  if (!iterator->ReadInt(&index_) ||
      !iterator->ReadString(&virtual_url_spec) ||
      !iterator->ReadString16(&title_) ||
      !iterator->ReadString(&content_state_) ||
      !iterator->ReadInt(&transition_type_int))
    return false;
  virtual_url_ = GURL(virtual_url_spec);
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
    WebKit::WebReferrerPolicy policy;
    if (iterator->ReadInt(&policy_int))
      policy = static_cast<WebKit::WebReferrerPolicy>(policy_int);
    else
      policy = WebKit::WebReferrerPolicyDefault;
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
  }

  return true;
}

scoped_ptr<NavigationEntry> TabNavigation::ToNavigationEntry(
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
  entry->SetContentState(content_state_);
  entry->SetPageID(page_id);
  entry->SetHasPostData(has_post_data_);
  entry->SetPostID(post_id_);
  entry->SetOriginalRequestURL(original_request_url_);
  entry->SetIsOverridingUserAgent(is_overriding_user_agent_);
  entry->SetTimestamp(timestamp_);
  entry->SetExtraData(chrome::search::kInstantExtendedSearchTermsKey,
                      search_terms_);

  return entry.Pass();
}

// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
sync_pb::TabNavigation TabNavigation::ToSyncData() const {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(virtual_url_.spec());
  // FIXME(zea): Support referrer policy?
  sync_data.set_referrer(referrer_.url.spec());
  sync_data.set_title(UTF16ToUTF8(title_));

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

  sync_data.set_search_terms(UTF16ToUTF8(search_terms_));

  if (favicon_url_.is_valid())
    sync_data.set_favicon_url(favicon_url_.spec());

  return sync_data;
}

// static
std::vector<NavigationEntry*>
TabNavigation::CreateNavigationEntriesFromTabNavigations(
    const std::vector<TabNavigation>& navigations,
    content::BrowserContext* browser_context) {
  int page_id = 0;
  std::vector<NavigationEntry*> entries;
  for (std::vector<TabNavigation>::const_iterator it = navigations.begin();
       it != navigations.end(); ++it) {
    entries.push_back(
        it->ToNavigationEntry(page_id, browser_context).release());
    ++page_id;
  }
  return entries;
}

// SessionTab -----------------------------------------------------------------

SessionTab::SessionTab()
    : tab_visual_index(-1),
      current_navigation_index(-1),
      pinned(false) {
}

SessionTab::~SessionTab() {
}

void SessionTab::SetFromSyncData(const sync_pb::SessionTab& sync_data,
                                 base::Time timestamp) {
  window_id.set_id(sync_data.window_id());
  tab_id.set_id(sync_data.tab_id());
  tab_visual_index = sync_data.tab_visual_index();
  current_navigation_index = sync_data.current_navigation_index();
  pinned = sync_data.pinned();
  extension_app_id = sync_data.extension_app_id();
  user_agent_override.clear();
  this->timestamp = timestamp;
  navigations.clear();
  for (int i = 0; i < sync_data.navigation_size(); ++i) {
    navigations.push_back(
        TabNavigation::FromSyncData(i, sync_data.navigation(i)));
  }
  session_storage_persistent_id.clear();
}

sync_pb::SessionTab SessionTab::ToSyncData() const {
  sync_pb::SessionTab sync_data;
  sync_data.set_tab_id(tab_id.id());
  sync_data.set_window_id(window_id.id());
  sync_data.set_tab_visual_index(tab_visual_index);
  sync_data.set_current_navigation_index(current_navigation_index);
  sync_data.set_pinned(pinned);
  sync_data.set_extension_app_id(extension_app_id);
  for (std::vector<TabNavigation>::const_iterator it = navigations.begin();
       it != navigations.end(); ++it) {
    *sync_data.add_navigation() = it->ToSyncData();
  }
  return sync_data;
}

// SessionWindow ---------------------------------------------------------------

SessionWindow::SessionWindow()
    : selected_tab_index(-1),
      type(Browser::TYPE_TABBED),
      is_constrained(true),
      show_state(ui::SHOW_STATE_DEFAULT) {
}

SessionWindow::~SessionWindow() {
  STLDeleteElements(&tabs);
}
