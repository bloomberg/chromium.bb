// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/search_action.h"

#include <set>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "jni/SearchAction_jni.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using content::WebContents;

namespace {
// Context length is set to 1.5K characters for historical reasons:
// This provided the largest context that would fit in a GET request along with
// our other required parameters."
const int kSurroundingTextSize = 1536;
}

SearchAction::SearchAction(JNIEnv* env, jobject obj) {
  java_object_.Reset(env, obj);
}

SearchAction::~SearchAction() {
  // The Java object can be null in tests only, by calling the default
  // constructor.
  if (!java_object_.is_null()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_SearchAction_clearNativePointer(env, java_object_.obj());
  }
}

void SearchAction::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void SearchAction::RequestSurroundingText(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  GURL url(web_contents->GetURL());
  std::string encoding(web_contents->GetEncoding());
  search_context_.reset(new ContextualSearchContext(true, url, encoding));

  content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  if (!focused_frame) {
    OnSurroundingTextResponse(base::string16(), 0, 0);
  } else {
    focused_frame->RequestTextSurroundingSelection(
        base::Bind(&SearchAction::OnSurroundingTextResponse, AsWeakPtr()),
        kSurroundingTextSize);
  }
}

void SearchAction::OnSurroundingTextResponse(
    const base::string16& surrounding_text,
    int focus_start,
    int focus_end) {
  // Record the context, which can be accessed later on demand.
  search_context_->surrounding_text = surrounding_text;
  search_context_->start_offset = focus_start;
  search_context_->end_offset = focus_end;

  // Notify Java.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SearchAction_onSurroundingTextReady(env, java_object_.obj());
}

std::string SearchAction::FindFocusedWord() {
  DCHECK(search_context_);

  const base::string16& surrounding_text = search_context_->surrounding_text;
  int focus_start = search_context_->start_offset;
  int focus_end = search_context_->end_offset;
  int surrounding_text_length = surrounding_text.length();

  // First, we need to find the focused word boundaries.
  int focused_word_start = focus_start;
  int focused_word_end = focus_end;

  if (surrounding_text_length > 0 &&
      IsValidCharacter(surrounding_text[focused_word_start])) {
    // Find focused word start (inclusive)
    for (int i = focus_start; i >= 0; i--) {
      focused_word_start = i;

      wchar_t character;
      if (i > 0) {
        character = surrounding_text[i - 1];

        if (!IsValidCharacter(character))
          break;
      }
    }

    // Find focused word end (non inclusive)
    for (int i = focus_end; i < surrounding_text_length; i++) {
      wchar_t character = surrounding_text[i];

      if (IsValidCharacter(character)) {
        focused_word_end = i + 1;
      } else {
        focused_word_end = i;
        break;
      }
    }
  }

  return base::UTF16ToUTF8(surrounding_text.substr(
      focused_word_start, focused_word_end - focused_word_start));
}

std::string SearchAction::GetSampleText(int sample_length) {
  DCHECK(search_context_);

  const base::string16& surrounding_text = search_context_->surrounding_text;
  int focus_start = search_context_->start_offset;
  int focus_end = search_context_->end_offset;
  int surrounding_text_length = surrounding_text.length();

  //---------------------------------------------------------------------------
  // Cut the surrounding size so it can be sent to the Java side.

  // Start equally distributing the size around the focal point.
  int focal_point_size = focus_end - focus_start;
  int sample_margin = (sample_length - focal_point_size) / 2;
  int sample_start = focus_start - sample_margin;
  int sample_end = focus_end + sample_margin;

  // If the the start is out of bounds, compensate the end side.
  if (sample_start < 0) {
    sample_end -= sample_start;
    sample_start = 0;
  }

  // If the the end is out of bounds, compensate the start side.
  if (sample_end > surrounding_text_length) {
    int diff = sample_end - surrounding_text_length;
    sample_end = surrounding_text_length;
    sample_start -= diff;
  }

  // Trim the start and end to make sure they are within bounds.
  sample_start = std::max(0, sample_start);
  sample_end = std::min(surrounding_text_length, sample_end);
  return base::UTF16ToUTF8(
      surrounding_text.substr(sample_start, sample_end - sample_start));
}

bool SearchAction::IsValidCharacter(int char_code) {
  // See http://www.unicode.org/Public/UCD/latest/ucd/NamesList.txt
  // See http://jrgraphix.net/research/unicode_blocks.php

  // TODO(donnd): should not include CJK characters!
  // TODO(donnd): consider using regular expressions.

  if ((char_code >= 11264 && char_code <= 55295) ||  // Asian language symbols
      (char_code >= 192 && char_code <= 8191) ||  // Letters in many languages
      (char_code >= 97 && char_code <= 122) ||    // Lowercase letters
      (char_code >= 65 && char_code <= 90) ||     // Uppercase letters
      (char_code >= 48 && char_code <= 57)) {     // Numbers
    return true;
  }

  return false;
}

// Testing

SearchAction::SearchAction() {}

void SearchAction::SetContext(std::string surrounding_text,
                              int focus_start,
                              int focus_end) {
  search_context_.reset(new ContextualSearchContext(false, GURL(), ""));
  search_context_->surrounding_text = base::UTF8ToUTF16(surrounding_text);
  search_context_->start_offset = focus_start;
  search_context_->end_offset = focus_end;
}

// Boilerplate

bool RegisterSearchAction(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

jlong Init(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  SearchAction* content = new SearchAction(env, obj);
  return reinterpret_cast<intptr_t>(content);
}
