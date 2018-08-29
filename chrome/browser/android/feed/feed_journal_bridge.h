// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_FEED_JOURNAL_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_FEED_FEED_JOURNAL_BRIDGE_H_

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"

namespace feed {

class FeedJournalDatabase;

// Native counterpart of FeedJournalBridge.java. Holds non-owning pointers
// to native implementation to which operations are delegated. Results are
// passed back by a single argument callback so
// base::android::RunBooleanCallbackAndroid() and
// base::android::RunObjectCallbackAndroid() can be used. This bridge is
// instantiated, owned, and destroyed from Java.
class FeedJournalBridge {
 public:
  explicit FeedJournalBridge(FeedJournalDatabase* feed_Storage_database);
  ~FeedJournalBridge();

  void Destroy(JNIEnv* j_env, const base::android::JavaRef<jobject>& j_this);

  void LoadJournal(JNIEnv* j_env,
                   const base::android::JavaRef<jobject>& j_this,
                   const base::android::JavaRef<jstring>& j_journal_name,
                   const base::android::JavaRef<jobject>& j_callback);
  void CommitJournalMutation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this,
                             const base::android::JavaRef<jobject>& j_callback);
  void DoesJournalExist(JNIEnv* j_env,
                        const base::android::JavaRef<jobject>& j_this,
                        const base::android::JavaRef<jstring>& j_journal_name,
                        const base::android::JavaRef<jobject>& j_callback);
  void LoadAllJournalKeys(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const base::android::JavaRef<jobject>& j_callback);
  void DeleteAllJournals(JNIEnv* j_env,
                         const base::android::JavaRef<jobject>& j_this,
                         const base::android::JavaRef<jobject>& j_callback);

  void CreateJournalMutation(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_journal_name);
  void DeleteJournalMutation(JNIEnv* j_env,
                             const base::android::JavaRef<jobject>& j_this);
  void AddAppendOperation(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this,
                          const base::android::JavaRef<jstring>& j_value);
  void AddCopyOperation(
      JNIEnv* j_env,
      const base::android::JavaRef<jobject>& j_this,
      const base::android::JavaRef<jstring>& j_to_journal_name);
  void AddDeleteOperation(JNIEnv* j_env,
                          const base::android::JavaRef<jobject>& j_this);

 private:
  void OnLoadJournalDone(base::android::ScopedJavaGlobalRef<jobject> callback,
                         std::vector<std::string> entries);

  FeedJournalDatabase* feed_journal_database_;

  base::WeakPtrFactory<FeedJournalBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedJournalBridge);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_FEED_JOURNAL_BRIDGE_H_
