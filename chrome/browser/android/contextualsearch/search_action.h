// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SEARCH_ACTION_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SEARCH_ACTION_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"

struct ContextualSearchContext;

// Represents the native side of a Java Search Action, which knows how to do a
// Contextual Search.
// This is part of the 2016-refactoring (crbug.com/624609,
// go/cs-refactoring-2016).
// Gathers text surrounding the selection from the page and makes it accessible
// to Java.
// TODO(donnd): add capability to "resolve" the best search term for the section
// of the page based on a server request or local text analysis.
class SearchAction : public base::SupportsWeakPtr<SearchAction> {
 public:
  // Constructs a new Search Action linked to the given Java object.
  SearchAction(JNIEnv* env, jobject obj);
  virtual ~SearchAction();

  // Should be called when this native object is no longer needed (calls the
  // destructor).
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  // Requests the text surrounding the selection for the given WebContents Java
  // object.  The surrounding text will be made available through a call to
  // |OnSurroundingTextResponse|.
  void RequestSurroundingText(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_web_contents);

  // Finds and returns the focused word within the current surrounding text.
  // RequestSurroundingText must be called first and the associated response
  // received.
  std::string FindFocusedWord();

  // Returns a sample of the current surrounding text of the given
  // |sample_length| or shorter if insufficient sample text is available.
  // RequestSurroundingText must be called first and the associated response
  // received.
  std::string GetSampleText(int sample_length);

 private:
  friend class SearchActionTest;
  FRIEND_TEST_ALL_PREFIXES(SearchActionTest, IsValidCharacterTest);
  FRIEND_TEST_ALL_PREFIXES(SearchActionTest, FindFocusedWordTest);
  FRIEND_TEST_ALL_PREFIXES(SearchActionTest, SampleSurroundingsTest);

  // Analyzes the surrounding text and makes it available to the Java
  // SearchAction object in a call to SearchAction#onSurroundingTextResponse.
  void OnSurroundingTextResponse(const base::string16& surrounding_text,
                                 int focus_start,
                                 int focus_end);

  // Determines if the given character is a valid part of a word to search for.
  bool IsValidCharacter(int char_code);

  // Testing support
  SearchAction();
  void SetContext(std::string surrounding_text, int focus_start, int focus_end);

  // The linked Java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // The current search context, or null.
  std::shared_ptr<ContextualSearchContext> search_context_;

  DISALLOW_COPY_AND_ASSIGN(SearchAction);
};

bool RegisterSearchAction(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_SEARCH_ACTION_H_
