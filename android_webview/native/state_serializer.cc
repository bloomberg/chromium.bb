// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/state_serializer.h"

#include <string>

#include "base/pickle.h"
#include "base/time/time.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_state.h"

// Reasons for not re-using TabNavigation under chrome/ as of 20121116:
// * Android WebView has different requirements for fields to store since
//   we are the only ones using values like BaseURLForDataURL.
// * TabNavigation does unnecessary copying of data, which in Android
//   WebView case, is undesired since save/restore is called in Android
//   very frequently.
// * TabNavigation is tightly integrated with the rest of chrome session
//   restore and sync code, and has other purpose in addition to serializing
//   NavigationEntry.

using std::string;

namespace android_webview {

namespace {

const uint32_t AW_STATE_VERSION = internal::AW_STATE_VERSION_DATA_URL;

}  // namespace

bool WriteToPickle(const content::WebContents& web_contents,
                   base::Pickle* pickle) {
  DCHECK(pickle);

  if (!internal::WriteHeaderToPickle(pickle))
    return false;

  const content::NavigationController& controller =
      web_contents.GetController();
  const int entry_count = controller.GetEntryCount();
  const int selected_entry = controller.GetCurrentEntryIndex();
  DCHECK_GE(entry_count, 0);
  DCHECK_GE(selected_entry, -1);  // -1 is valid
  DCHECK_LT(selected_entry, entry_count);

  if (!pickle->WriteInt(entry_count))
    return false;

  if (!pickle->WriteInt(selected_entry))
    return false;

  for (int i = 0; i < entry_count; ++i) {
    if (!internal::WriteNavigationEntryToPickle(*controller.GetEntryAtIndex(i),
                                                pickle))
      return false;
  }

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.

  return true;
}

bool RestoreFromPickle(base::PickleIterator* iterator,
                       content::WebContents* web_contents) {
  DCHECK(iterator);
  DCHECK(web_contents);

  uint32_t state_version = internal::RestoreHeaderFromPickle(iterator);
  if (!state_version)
    return false;

  int entry_count = -1;
  int selected_entry = -2;  // -1 is a valid value

  if (!iterator->ReadInt(&entry_count))
    return false;

  if (!iterator->ReadInt(&selected_entry))
    return false;

  if (entry_count < 0)
    return false;
  if (selected_entry < -1)
    return false;
  if (selected_entry >= entry_count)
    return false;

  std::vector<scoped_ptr<content::NavigationEntry>> entries;
  entries.reserve(entry_count);
  for (int i = 0; i < entry_count; ++i) {
    entries.push_back(content::NavigationEntry::Create());
    if (!internal::RestoreNavigationEntryFromPickle(state_version, iterator,
                                                    entries[i].get()))
      return false;

    entries[i]->SetPageID(i);
  }

  // |web_contents| takes ownership of these entries after this call.
  content::NavigationController& controller = web_contents->GetController();
  controller.Restore(
      selected_entry,
      content::NavigationController::RESTORE_LAST_SESSION_EXITED_CLEANLY,
      &entries);
  DCHECK_EQ(0u, entries.size());

  if (controller.GetLastCommittedEntry()) {
    // Set up the file access rights for the selected navigation entry.
    // TODO(joth): This is duplicated from chrome/.../session_restore.cc and
    // should be shared e.g. in  NavigationController. http://crbug.com/68222
    const int id = web_contents->GetRenderProcessHost()->GetID();
    const content::PageState& page_state =
        controller.GetLastCommittedEntry()->GetPageState();
    const std::vector<base::FilePath>& file_paths =
        page_state.GetReferencedFiles();
    for (std::vector<base::FilePath>::const_iterator file = file_paths.begin();
         file != file_paths.end(); ++file) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantReadFile(id,
                                                                        *file);
    }
  }

  controller.LoadIfNecessary();

  return true;
}

namespace internal {

bool WriteHeaderToPickle(base::Pickle* pickle) {
  return WriteHeaderToPickle(AW_STATE_VERSION, pickle);
}

bool WriteHeaderToPickle(uint32_t state_version, base::Pickle* pickle) {
  return pickle->WriteUInt32(state_version);
}

uint32_t RestoreHeaderFromPickle(base::PickleIterator* iterator) {
  uint32_t state_version = -1;
  if (!iterator->ReadUInt32(&state_version))
    return 0;

  if (IsSupportedVersion(state_version)) {
    return state_version;
  }

  return 0;
}

bool IsSupportedVersion(uint32_t state_version) {
  return state_version == internal::AW_STATE_VERSION_INITIAL ||
         state_version == internal::AW_STATE_VERSION_DATA_URL;
}

bool WriteNavigationEntryToPickle(const content::NavigationEntry& entry,
                                  base::Pickle* pickle) {
  return WriteNavigationEntryToPickle(AW_STATE_VERSION, entry, pickle);
}

bool WriteNavigationEntryToPickle(uint32_t state_version,
                                  const content::NavigationEntry& entry,
                                  base::Pickle* pickle) {
  DCHECK(IsSupportedVersion(state_version));
  if (!pickle->WriteString(entry.GetURL().spec()))
    return false;

  if (!pickle->WriteString(entry.GetVirtualURL().spec()))
    return false;

  const content::Referrer& referrer = entry.GetReferrer();
  if (!pickle->WriteString(referrer.url.spec()))
    return false;
  if (!pickle->WriteInt(static_cast<int>(referrer.policy)))
    return false;

  if (!pickle->WriteString16(entry.GetTitle()))
    return false;

  if (!pickle->WriteString(entry.GetPageState().ToEncodedData()))
    return false;

  if (!pickle->WriteBool(static_cast<int>(entry.GetHasPostData())))
    return false;

  if (!pickle->WriteString(entry.GetOriginalRequestURL().spec()))
    return false;

  if (!pickle->WriteString(entry.GetBaseURLForDataURL().spec()))
    return false;

  if (state_version >= internal::AW_STATE_VERSION_DATA_URL) {
    const char* data = nullptr;
    size_t size = 0;
    scoped_refptr<const base::RefCountedString> s = entry.GetDataURLAsString();
    if (s) {
      data = s->front_as<char>();
      size = s->size();
    }
    // Even when |entry.GetDataForDataURL()| is null we still need to write a
    // zero-length entry to ensure the fields all line up when read back in.
    if (!pickle->WriteData(data, size))
      return false;
  }

  if (!pickle->WriteBool(static_cast<int>(entry.GetIsOverridingUserAgent())))
    return false;

  if (!pickle->WriteInt64(entry.GetTimestamp().ToInternalValue()))
    return false;

  if (!pickle->WriteInt(entry.GetHttpStatusCode()))
    return false;

  // Please update AW_STATE_VERSION and IsSupportedVersion() if serialization
  // format is changed.
  // Make sure the serialization format is updated in a backwards compatible
  // way.

  return true;
}

bool RestoreNavigationEntryFromPickle(base::PickleIterator* iterator,
                                      content::NavigationEntry* entry) {
  return RestoreNavigationEntryFromPickle(AW_STATE_VERSION, iterator, entry);
}

bool RestoreNavigationEntryFromPickle(uint32_t state_version,
                                      base::PickleIterator* iterator,
                                      content::NavigationEntry* entry) {
  DCHECK(IsSupportedVersion(state_version));
  {
    string url;
    if (!iterator->ReadString(&url))
      return false;
    entry->SetURL(GURL(url));
  }

  {
    string virtual_url;
    if (!iterator->ReadString(&virtual_url))
      return false;
    entry->SetVirtualURL(GURL(virtual_url));
  }

  {
    content::Referrer referrer;
    string referrer_url;
    int policy;

    if (!iterator->ReadString(&referrer_url))
      return false;
    if (!iterator->ReadInt(&policy))
      return false;

    referrer.url = GURL(referrer_url);
    referrer.policy = static_cast<blink::WebReferrerPolicy>(policy);
    entry->SetReferrer(referrer);
  }

  {
    base::string16 title;
    if (!iterator->ReadString16(&title))
      return false;
    entry->SetTitle(title);
  }

  {
    string content_state;
    if (!iterator->ReadString(&content_state))
      return false;
    entry->SetPageState(
        content::PageState::CreateFromEncodedData(content_state));
  }

  {
    bool has_post_data;
    if (!iterator->ReadBool(&has_post_data))
      return false;
    entry->SetHasPostData(has_post_data);
  }

  {
    string original_request_url;
    if (!iterator->ReadString(&original_request_url))
      return false;
    entry->SetOriginalRequestURL(GURL(original_request_url));
  }

  {
    string base_url_for_data_url;
    if (!iterator->ReadString(&base_url_for_data_url))
      return false;
    entry->SetBaseURLForDataURL(GURL(base_url_for_data_url));
  }

  if (state_version >= internal::AW_STATE_VERSION_DATA_URL) {
    const char* data;
    int size;
    if (!iterator->ReadData(&data, &size))
      return false;
    if (size > 0) {
      scoped_refptr<base::RefCountedString> ref = new base::RefCountedString();
      ref->data().assign(data, size);
      entry->SetDataURLAsString(ref);
    }
  }

  {
    bool is_overriding_user_agent;
    if (!iterator->ReadBool(&is_overriding_user_agent))
      return false;
    entry->SetIsOverridingUserAgent(is_overriding_user_agent);
  }

  {
    int64_t timestamp;
    if (!iterator->ReadInt64(&timestamp))
      return false;
    entry->SetTimestamp(base::Time::FromInternalValue(timestamp));
  }

  {
    int http_status_code;
    if (!iterator->ReadInt(&http_status_code))
      return false;
    entry->SetHttpStatusCode(http_status_code);
  }

  return true;
}

}  // namespace internal

}  // namespace android_webview
