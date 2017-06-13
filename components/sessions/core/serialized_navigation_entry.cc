// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/serialized_navigation_entry.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "components/sessions/core/serialized_navigation_driver.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/session_specifics.pb.h"

namespace sessions {

// TODO(treib): Remove, not needed anymore. crbug.com/627747
const char kSearchTermsKey[] = "search_terms";

// The previous referrer policy value corresponding to |Never|.
const int kObsoleteReferrerPolicyNever = 2;

SerializedNavigationEntry::SerializedNavigationEntry()
    : index_(-1),
      unique_id_(0),
      transition_type_(ui::PAGE_TRANSITION_TYPED),
      has_post_data_(false),
      post_id_(-1),
      is_overriding_user_agent_(false),
      http_status_code_(0),
      is_restored_(false),
      blocked_state_(STATE_INVALID),
      password_state_(PASSWORD_STATE_UNKNOWN) {
  referrer_policy_ =
      SerializedNavigationDriver::Get()->GetDefaultReferrerPolicy();
}

SerializedNavigationEntry::SerializedNavigationEntry(
    const SerializedNavigationEntry& other) = default;

SerializedNavigationEntry::~SerializedNavigationEntry() {}

SerializedNavigationEntry SerializedNavigationEntry::FromSyncData(
    int index,
    const sync_pb::TabNavigation& sync_data) {
  SerializedNavigationEntry navigation;
  navigation.index_ = index;
  navigation.unique_id_ = sync_data.unique_id();
  if (sync_data.has_correct_referrer_policy()) {
    navigation.referrer_url_ = GURL(sync_data.referrer());
    navigation.referrer_policy_ = sync_data.correct_referrer_policy();
  } else {
    navigation.referrer_url_ = GURL();
    navigation.referrer_policy_ = kObsoleteReferrerPolicyNever;
  }
  navigation.virtual_url_ = GURL(sync_data.virtual_url());
  navigation.title_ = base::UTF8ToUTF16(sync_data.title());

  uint32_t transition = 0;
  if (sync_data.has_page_transition()) {
    switch (sync_data.page_transition()) {
      case sync_pb::SyncEnums_PageTransition_LINK:
        transition = ui::PAGE_TRANSITION_LINK;
        break;
      case sync_pb::SyncEnums_PageTransition_TYPED:
        transition = ui::PAGE_TRANSITION_TYPED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK:
        transition = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME:
        transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME:
        transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
        break;
      case sync_pb::SyncEnums_PageTransition_GENERATED:
        transition = ui::PAGE_TRANSITION_GENERATED;
        break;
      case sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL:
        transition = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
        break;
      case sync_pb::SyncEnums_PageTransition_FORM_SUBMIT:
        transition = ui::PAGE_TRANSITION_FORM_SUBMIT;
        break;
      case sync_pb::SyncEnums_PageTransition_RELOAD:
        transition = ui::PAGE_TRANSITION_RELOAD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD:
        transition = ui::PAGE_TRANSITION_KEYWORD;
        break;
      case sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED:
        transition = ui::PAGE_TRANSITION_KEYWORD_GENERATED;
        break;
      default:
        transition = ui::PAGE_TRANSITION_LINK;
        break;
    }
  }

  if  (sync_data.has_redirect_type()) {
    switch (sync_data.redirect_type()) {
      case sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT:
        transition |= ui::PAGE_TRANSITION_CLIENT_REDIRECT;
        break;
      case sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT:
        transition |= ui::PAGE_TRANSITION_SERVER_REDIRECT;
        break;
    }
  }
  if (sync_data.navigation_forward_back())
      transition |= ui::PAGE_TRANSITION_FORWARD_BACK;
  if (sync_data.navigation_from_address_bar())
      transition |= ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  if (sync_data.navigation_home_page())
      transition |= ui::PAGE_TRANSITION_HOME_PAGE;
  if (sync_data.navigation_chain_start())
      transition |= ui::PAGE_TRANSITION_CHAIN_START;
  if (sync_data.navigation_chain_end())
      transition |= ui::PAGE_TRANSITION_CHAIN_END;

  navigation.transition_type_ = static_cast<ui::PageTransition>(transition);

  navigation.timestamp_ = base::Time();
  navigation.search_terms_ = base::UTF8ToUTF16(sync_data.search_terms());
  if (sync_data.has_favicon_url())
    navigation.favicon_url_ = GURL(sync_data.favicon_url());

  if (sync_data.has_password_state()) {
    navigation.password_state_ =
        static_cast<SerializedNavigationEntry::PasswordState>(
            sync_data.password_state());
  }

  navigation.http_status_code_ = sync_data.http_status_code();

  SerializedNavigationDriver::Get()->Sanitize(&navigation);

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
void WriteStringToPickle(base::Pickle* pickle,
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
void WriteString16ToPickle(base::Pickle* pickle,
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
// encoded_page_state_
// transition_type_
//
// Added on later:
//
// type_mask (has_post_data_)
// referrer_url_
// referrer_policy_ (broken, crbug.com/450589)
// original_request_url_
// is_overriding_user_agent_
// timestamp_
// search_terms_
// http_status_code_
// referrer_policy_
// extended_info_map_

void SerializedNavigationEntry::WriteToPickle(int max_size,
                                              base::Pickle* pickle) const {
  pickle->WriteInt(index_);

  int bytes_written = 0;

  WriteStringToPickle(pickle, &bytes_written, max_size,
                      virtual_url_.spec());

  WriteString16ToPickle(pickle, &bytes_written, max_size, title_);

  const std::string encoded_page_state =
      SerializedNavigationDriver::Get()->GetSanitizedPageStateForPickle(this);
  WriteStringToPickle(pickle, &bytes_written, max_size, encoded_page_state);

  pickle->WriteInt(transition_type_);

  const int type_mask = has_post_data_ ? HAS_POST_DATA : 0;
  pickle->WriteInt(type_mask);

  WriteStringToPickle(pickle, &bytes_written, max_size, referrer_url_.spec());

  // This field was deprecated in m61, but we still write it to the pickle for
  // forwards compatibility.
  pickle->WriteInt(kObsoleteReferrerPolicyNever);

  // Save info required to override the user agent.
  WriteStringToPickle(
      pickle, &bytes_written, max_size,
      original_request_url_.is_valid() ?
      original_request_url_.spec() : std::string());
  pickle->WriteBool(is_overriding_user_agent_);
  pickle->WriteInt64(timestamp_.ToInternalValue());

  WriteString16ToPickle(pickle, &bytes_written, max_size, search_terms_);

  pickle->WriteInt(http_status_code_);

  pickle->WriteInt(referrer_policy_);

  pickle->WriteInt(extended_info_map_.size());
  for (const auto entry : extended_info_map_) {
    WriteStringToPickle(pickle, &bytes_written, max_size, entry.first);
    WriteStringToPickle(pickle, &bytes_written, max_size, entry.second);
  }
}

bool SerializedNavigationEntry::ReadFromPickle(base::PickleIterator* iterator) {
  *this = SerializedNavigationEntry();
  std::string virtual_url_spec;
  int transition_type_int = 0;
  if (!iterator->ReadInt(&index_) ||
      !iterator->ReadString(&virtual_url_spec) ||
      !iterator->ReadString16(&title_) ||
      !iterator->ReadString(&encoded_page_state_) ||
      !iterator->ReadInt(&transition_type_int))
    return false;
  virtual_url_ = GURL(virtual_url_spec);
  transition_type_ = ui::PageTransitionFromInt(transition_type_int);

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
    referrer_url_ = GURL(referrer_spec);

    // Note: due to crbug.com/450589 the initial referrer policy is incorrect,
    // and ignored. A correct referrer policy is extracted later (see
    // |correct_referrer_policy| below).
    int ignored_referrer_policy;
    ignore_result(iterator->ReadInt(&ignored_referrer_policy));

    // If the original URL can't be found, leave it empty.
    std::string original_request_url_spec;
    if (!iterator->ReadString(&original_request_url_spec))
      original_request_url_spec = std::string();
    original_request_url_ = GURL(original_request_url_spec);

    // Default to not overriding the user agent if we don't have info.
    if (!iterator->ReadBool(&is_overriding_user_agent_))
      is_overriding_user_agent_ = false;

    int64_t timestamp_internal_value = 0;
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

    // Correct referrer policy (if present).
    int correct_referrer_policy;
    if (iterator->ReadInt(&correct_referrer_policy)) {
      referrer_policy_ = correct_referrer_policy;
    } else {
      encoded_page_state_ =
          SerializedNavigationDriver::Get()->StripReferrerFromPageState(
              encoded_page_state_);
    }

    int extended_info_map_size = 0;
    if (iterator->ReadInt(&extended_info_map_size) &&
        extended_info_map_size > 0) {
      for (int i = 0; i < extended_info_map_size; ++i) {
        std::string key;
        std::string value;
        if (iterator->ReadString(&key) && iterator->ReadString(&value))
          extended_info_map_[key] = value;
      }
    }
  }

  SerializedNavigationDriver::Get()->Sanitize(this);

  is_restored_ = true;

  return true;
}

// TODO(zea): perhaps sync state (scroll position, form entries, etc.) as well?
// See http://crbug.com/67068.
sync_pb::TabNavigation SerializedNavigationEntry::ToSyncData() const {
  sync_pb::TabNavigation sync_data;
  sync_data.set_virtual_url(virtual_url_.spec());
  sync_data.set_referrer(referrer_url_.spec());
  sync_data.set_correct_referrer_policy(referrer_policy_);
  sync_data.set_title(base::UTF16ToUTF8(title_));

  // Page transition core.
  static_assert(static_cast<int32_t>(ui::PAGE_TRANSITION_LAST_CORE) ==
                static_cast<int32_t>(ui::PAGE_TRANSITION_KEYWORD_GENERATED),
                "PAGE_TRANSITION_LAST_CORE must equal "
                "PAGE_TRANSITION_KEYWORD_GENERATED");
  switch (ui::PageTransitionStripQualifier(transition_type_)) {
    case ui::PAGE_TRANSITION_LINK:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_LINK);
      break;
    case ui::PAGE_TRANSITION_TYPED:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_TYPED);
      break;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      sync_data.set_page_transition(
          sync_pb::SyncEnums_PageTransition_AUTO_BOOKMARK);
      break;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_SUBFRAME);
      break;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_MANUAL_SUBFRAME);
      break;
    case ui::PAGE_TRANSITION_GENERATED:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_GENERATED);
      break;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_AUTO_TOPLEVEL);
      break;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_FORM_SUBMIT);
      break;
    case ui::PAGE_TRANSITION_RELOAD:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_RELOAD);
      break;
    case ui::PAGE_TRANSITION_KEYWORD:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD);
      break;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      sync_data.set_page_transition(
        sync_pb::SyncEnums_PageTransition_KEYWORD_GENERATED);
      break;
    default:
      NOTREACHED();
  }

  // Page transition qualifiers.
  if (ui::PageTransitionIsRedirect(transition_type_)) {
    if (transition_type_ & ui::PAGE_TRANSITION_CLIENT_REDIRECT) {
      sync_data.set_redirect_type(
        sync_pb::SyncEnums_PageTransitionRedirectType_CLIENT_REDIRECT);
    } else if (transition_type_ & ui::PAGE_TRANSITION_SERVER_REDIRECT) {
      sync_data.set_redirect_type(
        sync_pb::SyncEnums_PageTransitionRedirectType_SERVER_REDIRECT);
    }
  }
  sync_data.set_navigation_forward_back(
      (transition_type_ & ui::PAGE_TRANSITION_FORWARD_BACK) != 0);
  sync_data.set_navigation_from_address_bar(
      (transition_type_ & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR) != 0);
  sync_data.set_navigation_home_page(
      (transition_type_ & ui::PAGE_TRANSITION_HOME_PAGE) != 0);
  sync_data.set_navigation_chain_start(
      (transition_type_ & ui::PAGE_TRANSITION_CHAIN_START) != 0);
  sync_data.set_navigation_chain_end(
      (transition_type_ & ui::PAGE_TRANSITION_CHAIN_END) != 0);

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

  sync_data.set_password_state(
      static_cast<sync_pb::TabNavigation_PasswordState>(password_state_));

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

size_t SerializedNavigationEntry::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return
      EstimateMemoryUsage(referrer_url_) +
      EstimateMemoryUsage(virtual_url_) +
      EstimateMemoryUsage(title_) +
      EstimateMemoryUsage(encoded_page_state_) +
      EstimateMemoryUsage(original_request_url_) +
      EstimateMemoryUsage(search_terms_) +
      EstimateMemoryUsage(favicon_url_) +
      EstimateMemoryUsage(redirect_chain_) +
      EstimateMemoryUsage(content_pack_categories_) +
      EstimateMemoryUsage(extended_info_map_);
}

}  // namespace sessions
