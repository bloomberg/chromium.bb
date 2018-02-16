// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/spellcheck/common/spellcheck.mojom.h"

// A class used to interface between the Java class of the same name and the
// android SpellCheckHost.  This class receives text to be spellchecked, sends
// that text to the Java side via JNI to be spellchecked, and then sends those
// results to the renderer.
class SpellCheckerSessionBridge {
 public:
  SpellCheckerSessionBridge();
  ~SpellCheckerSessionBridge();

  using RequestTextCheckCallback =
      spellcheck::mojom::SpellCheckHost::RequestTextCheckCallback;

  // Receives text to be checked and sends it to Java to be spellchecked.
  void RequestTextCheck(const base::string16& text,
                        RequestTextCheckCallback callback);

  // Receives information from Java side about the typos in a given string
  // of text, processes these and sends them to the renderer.
  void ProcessSpellCheckResults(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jobj,
      const base::android::JavaParamRef<jintArray>& offset_array,
      const base::android::JavaParamRef<jintArray>& length_array,
      const base::android::JavaParamRef<jobjectArray>& suggestions_array);

  // Sets the handle to the Java SpellCheckerSessionBridge object to null,
  // marking the Java object for garbage collection.
  void DisconnectSession();

 private:
  class SpellingRequest {
   public:
    SpellingRequest(const base::string16& text,
                    RequestTextCheckCallback callback);
    ~SpellingRequest();

    base::string16 text_;
    RequestTextCheckCallback callback_;

   private:
    DISALLOW_COPY_AND_ASSIGN(SpellingRequest);
  };

  std::unique_ptr<SpellingRequest> active_request_;
  std::unique_ptr<SpellingRequest> pending_request_;

  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  bool java_object_initialization_failed_;
  bool active_session_;

  DISALLOW_COPY_AND_ASSIGN(SpellCheckerSessionBridge);
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECKER_SESSION_BRIDGE_ANDROID_H_
