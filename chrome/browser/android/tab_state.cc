// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_state.h"

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_command.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/web_contents.h"
#include "jni/TabState_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::NavigationController;
using content::WebContents;

namespace {

bool WriteStateHeaderToPickle(bool off_the_record,
                              int entry_count,
                              int current_entry_index,
                              base::Pickle* pickle) {
  return pickle->WriteBool(off_the_record) &&
      pickle->WriteInt(entry_count) &&
      pickle->WriteInt(current_entry_index);
}

// Migrates a pickled SerializedNavigationEntry from Android tab version 0 to
// 2 or (Chrome 18->26).
//
// Due to the fact that all SerializedNavigationEntrys were previously stored
// in a single pickle on Android, this function has to read the fields exactly
// how they were written on m18 which is a custom format and different other
// chromes.
//
// This uses the fields from SerializedNavigationEntry/TabNavigation from:
// https://gerrit-int.chromium.org/gitweb?p=clank/internal/apps.git;
//              a=blob;f=native/framework/chrome/tab.cc;hb=refs/heads/m18
//
// 1. For each tab navigation:
//   virtual_url
//   title
//   content_state
//   transition_type
//   type_mask
//
// 2. For each tab navigation:
//   referrer
//   is_overriding_user_agent
//
void UpgradeNavigationFromV0ToV2(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int entry_count,
    base::PickleIterator* iterator) {
  for (int i = 0; i < entry_count; ++i) {
    base::Pickle v2_pickle;
    std::string virtual_url_spec;
    std::string str_referrer;
    base::string16 title;
    std::string content_state;
    int transition_type_int;
    if (!iterator->ReadString(&virtual_url_spec) ||
        !iterator->ReadString(&str_referrer) ||
        !iterator->ReadString16(&title) ||
        !iterator->ReadString(&content_state) ||
        !iterator->ReadInt(&transition_type_int))
      return;

    // Write back the fields that were just read.
    v2_pickle.WriteInt(i);
    v2_pickle.WriteString(virtual_url_spec);
    v2_pickle.WriteString16(title);
    v2_pickle.WriteString(content_state);
    v2_pickle.WriteInt(transition_type_int);

    // type_mask
    v2_pickle.WriteInt(0);
    // referrer_spec
    v2_pickle.WriteString(str_referrer);
    // policy_int
    v2_pickle.WriteInt(0);
    // original_request_url_spec
    v2_pickle.WriteString(std::string());
    // is_overriding_user_agent
    v2_pickle.WriteBool(false);
    // timestamp_internal_value
    v2_pickle.WriteInt64(0);
    // search_terms
    v2_pickle.WriteString16(base::string16());

    base::PickleIterator tab_navigation_pickle_iterator(v2_pickle);
    sessions::SerializedNavigationEntry nav;
    if (nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
      navigations->push_back(nav);
    } else {
      LOG(ERROR) << "Failed to read SerializedNavigationEntry from pickle "
                 << "(index=" << i << ", url=" << virtual_url_spec;
    }

  }

  for (int i = 0; i < entry_count; ++i) {
    std::string initial_url;
    bool user_agent_overridden;
    if (!iterator->ReadString(&initial_url) ||
        !iterator->ReadBool(&user_agent_overridden)) {
      break;
    }
  }
}

// Migrates a pickled SerializedNavigationEntry from Android tab version 0 to 1
// (or Chrome 25->26)
//
// Due to the fact that all SerializedNavigationEntrys were previously stored in
// a single pickle on Android, this function reads all the old fields,
// re-outputs them and appends an empty string16, representing the new
// search_terms field, and ensures that reading a v0 SerializedNavigationEntry
// won't consume bytes from a subsequent SerializedNavigationEntry.
//
// This uses the fields from SerializedNavigationEntry/TabNavigation prior to
// https://chromiumcodereview.appspot.com/11876045 which are:
//
// index
// virtual_url
// title
// content_state
// transition_type
// type_mask
// referrer
// original_request_url
// is_overriding_user_agent
// timestamp
//
// And finally search_terms was added and this function appends it.
void UpgradeNavigationFromV1ToV2(
    std::vector<sessions::SerializedNavigationEntry>* navigations,
    int entry_count,
    base::PickleIterator* iterator) {
  for (int i = 0; i < entry_count; ++i) {
    base::Pickle v2_pickle;

    int index;
    std::string virtual_url_spec;
    base::string16 title;
    std::string content_state;
    int transition_type_int;
    if (!iterator->ReadInt(&index) ||
        !iterator->ReadString(&virtual_url_spec) ||
        !iterator->ReadString16(&title) ||
        !iterator->ReadString(&content_state) ||
        !iterator->ReadInt(&transition_type_int))
      return;

    // Write back the fields that were just read.
    v2_pickle.WriteInt(index);
    v2_pickle.WriteString(virtual_url_spec);
    v2_pickle.WriteString16(title);
    v2_pickle.WriteString(content_state);
    v2_pickle.WriteInt(transition_type_int);

    int type_mask = 0;
    if (!iterator->ReadInt(&type_mask))
      continue;
    v2_pickle.WriteInt(type_mask);

    std::string referrer_spec;
    if (iterator->ReadString(&referrer_spec))
      v2_pickle.WriteString(referrer_spec);

    int policy_int;
    if (iterator->ReadInt(&policy_int))
      v2_pickle.WriteInt(policy_int);

    std::string original_request_url_spec;
    if (iterator->ReadString(&original_request_url_spec))
      v2_pickle.WriteString(original_request_url_spec);

    bool is_overriding_user_agent;
    if (iterator->ReadBool(&is_overriding_user_agent))
      v2_pickle.WriteBool(is_overriding_user_agent);

    int64_t timestamp_internal_value = 0;
    if (iterator->ReadInt64(&timestamp_internal_value))
      v2_pickle.WriteInt64(timestamp_internal_value);

    // Force output of search_terms
    v2_pickle.WriteString16(base::string16());

    base::PickleIterator tab_navigation_pickle_iterator(v2_pickle);
    sessions::SerializedNavigationEntry nav;
    if (nav.ReadFromPickle(&tab_navigation_pickle_iterator)) {
      navigations->push_back(nav);
    } else {
      LOG(ERROR) << "Failed to read SerializedNavigationEntry from pickle "
                 << "(index=" << i << ", url=" << virtual_url_spec;
    }
  }
}

// Extracts state and navigation entries from the given Pickle data and returns
// whether un-pickling the data succeeded
bool ExtractNavigationEntries(
    void* data,
    int size,
    int saved_state_version,
    bool* is_off_the_record,
    int* current_entry_index,
    std::vector<sessions::SerializedNavigationEntry>* navigations) {
  int entry_count;
  base::Pickle pickle(static_cast<char*>(data), size);
  base::PickleIterator iter(pickle);
  if (!iter.ReadBool(is_off_the_record) || !iter.ReadInt(&entry_count) ||
      !iter.ReadInt(current_entry_index)) {
    LOG(ERROR) << "Failed to restore state from byte array (length=" << size
               << ").";
    return false;
  }

  if (!saved_state_version) {
    // When |saved_state_version| is 0, it predates our notion of each tab
    // having a saved version id. For that version of tab serialization, we
    // used a single pickle for all |SerializedNavigationEntry|s.
    UpgradeNavigationFromV0ToV2(navigations, entry_count, &iter);
  } else if (saved_state_version == 1) {
    // When |saved_state_version| is 1, it predates our notion of each tab
    // having a saved version id. For that version of tab serialization, we
    // used a single pickle for all |SerializedNavigationEntry|s.
    UpgradeNavigationFromV1ToV2(navigations, entry_count, &iter);
  } else {
    // |saved_state_version| == 2 and greater.
    for (int i = 0; i < entry_count; ++i) {
      // Read each SerializedNavigationEntry as a separate pickle to avoid
      // optional reads of one tab bleeding into the next tab's data.
      int tab_navigation_data_length = 0;
      const char* tab_navigation_data = NULL;
      if (!iter.ReadInt(&tab_navigation_data_length) ||
          !iter.ReadBytes(&tab_navigation_data, tab_navigation_data_length)) {
        LOG(ERROR)
            << "Failed to restore tab entry from byte array. "
            << "(SerializedNavigationEntry size=" << tab_navigation_data_length
            << ").";
        return false;  // It's dangerous to keep deserializing now, give up.
      }
      base::Pickle tab_navigation_pickle(tab_navigation_data,
                                         tab_navigation_data_length);
      base::PickleIterator tab_navigation_pickle_iterator(
          tab_navigation_pickle);
      sessions::SerializedNavigationEntry nav;
      if (!nav.ReadFromPickle(&tab_navigation_pickle_iterator))
        return false;  // If we failed to read a navigation, give up on others.

      navigations->push_back(nav);
    }
  }

  // Validate the data.
  if (*current_entry_index < 0 ||
      *current_entry_index >= static_cast<int>(navigations->size()))
    return false;

  return true;
}

};  // anonymous namespace

ScopedJavaLocalRef<jobject> WebContentsState::GetContentsStateAsByteBuffer(
    JNIEnv* env,
    TabAndroid* tab) {
  Profile* profile = tab->GetProfile();
  if (!profile)
    return ScopedJavaLocalRef<jobject>();

  content::NavigationController& controller =
      tab->web_contents()->GetController();
  const int entry_count = controller.GetEntryCount();
  if (entry_count == 0)
    return ScopedJavaLocalRef<jobject>();

  std::vector<content::NavigationEntry*> navigations(entry_count);
  for (int i = 0; i < entry_count; ++i) {
    navigations[i] = controller.GetEntryAtIndex(i);
  }

  return WebContentsState::WriteNavigationsAsByteBuffer(
      env, profile->IsOffTheRecord(), navigations,
      controller.GetLastCommittedEntryIndex());
}

// Common implementation for GetContentsStateAsByteBuffer() and
// CreateContentsStateAsByteBuffer(). Does not assume ownership of the
// navigations.
ScopedJavaLocalRef<jobject> WebContentsState::WriteNavigationsAsByteBuffer(
    JNIEnv* env,
    bool is_off_the_record,
    const std::vector<content::NavigationEntry*>& navigations,
    int current_entry) {
  base::Pickle pickle;
  if (!WriteStateHeaderToPickle(is_off_the_record, navigations.size(),
                                current_entry, &pickle)) {
    LOG(ERROR) << "Failed to serialize tab state (entry count=" <<
        navigations.size() << ").";
    return ScopedJavaLocalRef<jobject>();
  }

  // Write out all of the NavigationEntrys.
  for (size_t i = 0; i < navigations.size(); ++i) {
    // Write each SerializedNavigationEntry as a separate pickle to avoid
    // optional reads of one tab bleeding into the next tab's data.
    base::Pickle tab_navigation_pickle;
    // Max size taken from BaseSessionService::CreateUpdateTabNavigationCommand.
    static const size_t max_state_size =
        std::numeric_limits<sessions::SessionCommand::size_type>::max() - 1024;
    sessions::ContentSerializedNavigationBuilder::FromNavigationEntry(
        i, *navigations[i])
        .WriteToPickle(max_state_size, &tab_navigation_pickle);
    pickle.WriteInt(tab_navigation_pickle.size());
    pickle.WriteBytes(tab_navigation_pickle.data(),
                      tab_navigation_pickle.size());
  }

  void* buffer = malloc(pickle.size());
  if (buffer == NULL) {
    // We can run out of memory allocating a large enough buffer.
    // In that case we'll only save the current entry.
    // TODO(jcivelli): http://b/issue?id=5869635 we should save more entries.
    // more TODO(jcivelli): Make this work
    return ScopedJavaLocalRef<jobject>();
  }
  // TODO(yfriedman): Add a |release| to Pickle and save the copy.
  memcpy(buffer, pickle.data(), pickle.size());
  ScopedJavaLocalRef<jobject> jb(env, env->NewDirectByteBuffer(buffer,
                                                               pickle.size()));
  if (base::android::ClearException(env) || jb.is_null())
    free(buffer);
  return jb;
}

ScopedJavaLocalRef<jstring>
WebContentsState::GetDisplayTitleFromByteBuffer(JNIEnv* env,
                                                void* data,
                                                int size,
                                                int saved_state_version) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = ExtractNavigationEntries(data,
                                          size,
                                          saved_state_version,
                                          &is_off_the_record,
                                          &current_entry_index,
                                          &navigations);
  if (!success)
    return ScopedJavaLocalRef<jstring>();

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return ConvertUTF16ToJavaString(env, nav_entry.title());
}

ScopedJavaLocalRef<jstring>
WebContentsState::GetVirtualUrlFromByteBuffer(JNIEnv* env,
                                              void* data,
                                              int size,
                                              int saved_state_version) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = ExtractNavigationEntries(data,
                                          size,
                                          saved_state_version,
                                          &is_off_the_record,
                                          &current_entry_index,
                                          &navigations);
  if (!success)
    return ScopedJavaLocalRef<jstring>();

  sessions::SerializedNavigationEntry nav_entry =
      navigations.at(current_entry_index);
  return ConvertUTF8ToJavaString(env, nav_entry.virtual_url().spec());
}

WebContents* WebContentsState::RestoreContentsFromByteBuffer(
    void* data,
    int size,
    int saved_state_version,
    bool initially_hidden) {
  bool is_off_the_record;
  int current_entry_index;
  std::vector<sessions::SerializedNavigationEntry> navigations;
  bool success = ExtractNavigationEntries(data,
                                          size,
                                          saved_state_version,
                                          &is_off_the_record,
                                          &current_entry_index,
                                          &navigations);
  if (!success)
    return NULL;

  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::vector<std::unique_ptr<content::NavigationEntry>> entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          navigations, profile);

  if (is_off_the_record)
    profile = profile->GetOffTheRecordProfile();
  WebContents::CreateParams params(profile);
  params.initially_hidden = initially_hidden;
  std::unique_ptr<WebContents> web_contents(WebContents::Create(params));
  web_contents->GetController().Restore(
      current_entry_index, content::RestoreType::CURRENT_SESSION, &entries);
  return web_contents.release();
}

ScopedJavaLocalRef<jobject> WebContentsState::RestoreContentsFromByteBuffer(
    JNIEnv* env,
    jclass clazz,
    jobject state,
    jint saved_state_version,
    jboolean initially_hidden) {
  void* data = env->GetDirectBufferAddress(state);
  int size = env->GetDirectBufferCapacity(state);

  WebContents* web_contents = WebContentsState::RestoreContentsFromByteBuffer(
      data,
      size,
      saved_state_version,
      initially_hidden);

  if (web_contents)
    return web_contents->GetJavaWebContents();
  else
    return ScopedJavaLocalRef<jobject>();
}

ScopedJavaLocalRef<jobject>
    WebContentsState::CreateSingleNavigationStateAsByteBuffer(
        JNIEnv* env,
        jstring url,
        jstring referrer_url,
        jint referrer_policy,
        jboolean is_off_the_record) {
  content::Referrer referrer;
  if (referrer_url) {
    referrer = content::Referrer(
        GURL(base::android::ConvertJavaStringToUTF8(env, referrer_url)),
        static_cast<blink::WebReferrerPolicy>(referrer_policy));
  }
  std::unique_ptr<content::NavigationEntry> entry(
      content::NavigationController::CreateNavigationEntry(
          GURL(base::android::ConvertJavaStringToUTF8(env, url)), referrer,
          ui::PAGE_TRANSITION_LINK,
          true,  // is_renderer_initiated
          "",    // extra_headers
          ProfileManager::GetActiveUserProfile()));

  std::vector<content::NavigationEntry*> navigations(1);
  navigations[0] = entry.get();

  return WebContentsState::WriteNavigationsAsByteBuffer(env,
                                                        is_off_the_record,
                                                        navigations,
                                                        0);
}

// Static JNI methods.

static void FreeWebContentsStateBuffer(JNIEnv* env,
                                       const JavaParamRef<jclass>& clazz,
                                       const JavaParamRef<jobject>& obj) {
  void* data = env->GetDirectBufferAddress(obj);
  free(data);
}

static ScopedJavaLocalRef<jobject> RestoreContentsFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& state,
    jint saved_state_version,
    jboolean initially_hidden) {
  return WebContentsState::RestoreContentsFromByteBuffer(env,
                                                         clazz,
                                                         state,
                                                         saved_state_version,
                                                         initially_hidden);
}

static ScopedJavaLocalRef<jobject> GetContentsStateAsByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& jtab) {
  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, jtab);
  return WebContentsState::GetContentsStateAsByteBuffer(env, tab_android);
}

static ScopedJavaLocalRef<jobject> CreateSingleNavigationStateAsByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url,
    const JavaParamRef<jstring>& referrer_url,
    jint referrer_policy,
    jboolean is_off_the_record) {
  return WebContentsState::CreateSingleNavigationStateAsByteBuffer(
      env, url, referrer_url, referrer_policy, is_off_the_record);
}

static ScopedJavaLocalRef<jstring> GetDisplayTitleFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& state,
    jint saved_state_version) {
  void* data = env->GetDirectBufferAddress(state);
  int size = env->GetDirectBufferCapacity(state);

  ScopedJavaLocalRef<jstring> result =
      WebContentsState::GetDisplayTitleFromByteBuffer(
          env, data, size, saved_state_version);
  return result;
}

static ScopedJavaLocalRef<jstring> GetVirtualUrlFromByteBuffer(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobject>& state,
    jint saved_state_version) {
  void* data = env->GetDirectBufferAddress(state);
  int size = env->GetDirectBufferCapacity(state);
  ScopedJavaLocalRef<jstring> result =
      WebContentsState::GetVirtualUrlFromByteBuffer(
          env, data, size, saved_state_version);
  return result;
}

// Creates a historical tab entry from the serialized tab contents contained
// within |state|.
static void CreateHistoricalTab(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                const JavaParamRef<jobject>& state,
                                jint saved_state_version) {
  std::unique_ptr<WebContents> web_contents(WebContents::FromJavaWebContents(
      WebContentsState::RestoreContentsFromByteBuffer(
          env, clazz, state, saved_state_version, true)));
  if (web_contents.get())
    TabAndroid::CreateHistoricalTabFromContents(web_contents.get());
}

bool RegisterTabState(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
