// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_

#include <string>

#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"

namespace content {

class ContentViewCore;
class RenderViewHost;

// Android implementation for DateTimeChooser dialogs.
class DateTimeChooserAndroid {
 public:
  DateTimeChooserAndroid();
  ~DateTimeChooserAndroid();

  // DateTimeChooser implementation:
  void ShowDialog(ContentViewCore* content,
                  RenderViewHost* sender,
                  int type, const std::string& value);

  // Replaces the current value with the one passed in text.
  void ReplaceDateTime(JNIEnv* env, jobject, jstring text);

  // Closes the dialog without propagating any changes.
  void CancelDialog(JNIEnv* env, jobject);

  // Propagates the different types of accepted date/time values to the
  // java side.
  static void InitializeDateInputTypes(
       int text_input_type_date, int text_input_type_date_time,
       int text_input_type_date_time_local, int text_input_type_month,
       int text_input_type_time);

 private:
  class DateTimeIPCSender;

  scoped_ptr<DateTimeIPCSender> communicator_;
  base::android::ScopedJavaGlobalRef<jobject> j_date_time_chooser_;

  DISALLOW_COPY_AND_ASSIGN(DateTimeChooserAndroid);
};

// Native JNI methods
bool RegisterDateTimeChooserAndroid(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_DATE_TIME_CHOOSER_ANDROID_H_
