// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/date_time_chooser_android.h"

#include "base/android/jni_string.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_view_host_observer.h"
#include "jni/DateTimeChooserAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;


namespace content {

// Updates date/time via IPC to the RenderView
class DateTimeChooserAndroid::DateTimeIPCSender :
    public RenderViewHostObserver {
 public:
  explicit DateTimeIPCSender(RenderViewHost* sender);
  virtual ~DateTimeIPCSender() {}
  void ReplaceDateTime(string16 text);
  void CancelDialog();

 private:
  DISALLOW_COPY_AND_ASSIGN(DateTimeIPCSender);
};

DateTimeChooserAndroid::DateTimeIPCSender::DateTimeIPCSender(
    RenderViewHost* sender)
  : RenderViewHostObserver(sender) {
}

void DateTimeChooserAndroid::DateTimeIPCSender::ReplaceDateTime(
    string16 text) {
  Send(new ViewMsg_ReplaceDateTime(routing_id(), text));
}

void DateTimeChooserAndroid::DateTimeIPCSender::CancelDialog() {
  Send(new ViewMsg_CancelDateTimeDialog(routing_id()));
}

// DateTimeChooserAndroid implementation
DateTimeChooserAndroid::DateTimeChooserAndroid() {
}

DateTimeChooserAndroid::~DateTimeChooserAndroid() {
}

// static
void DateTimeChooserAndroid::InitializeDateInputTypes(
      int text_input_type_date, int text_input_type_date_time,
      int text_input_type_date_time_local, int text_input_type_month,
      int text_input_type_time) {
  JNIEnv* env = AttachCurrentThread();
  Java_DateTimeChooserAndroid_initializeDateInputTypes(
         env,
         text_input_type_date, text_input_type_date_time,
         text_input_type_date_time_local, text_input_type_month,
         text_input_type_time);
}

void DateTimeChooserAndroid::ReplaceDateTime(
    JNIEnv* env, jobject, jstring text) {
  string16 text16 = ConvertJavaStringToUTF16(env, text);
  communicator_->ReplaceDateTime(text16);
}

void DateTimeChooserAndroid::CancelDialog(JNIEnv* env, jobject) {
  communicator_->CancelDialog();
}

void DateTimeChooserAndroid::ShowDialog(
    ContentViewCore* content, RenderViewHost* sender,
    int type, const std::string& value) {
  communicator_.reset(new DateTimeIPCSender(sender));
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> java_value =
      ConvertUTF8ToJavaString(env, value);
  j_date_time_chooser_.Reset(Java_DateTimeChooserAndroid_createDateTimeChooser(
      env, content->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this), java_value.obj(), type));
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------
bool RegisterDateTimeChooserAndroid(JNIEnv* env) {
  bool registered = RegisterNativesImpl(env);
  if (registered)
    DateTimeChooserAndroid::InitializeDateInputTypes(
        ui::TEXT_INPUT_TYPE_DATE,
        ui::TEXT_INPUT_TYPE_DATE_TIME,
        ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
        ui::TEXT_INPUT_TYPE_MONTH,
        ui::TEXT_INPUT_TYPE_TIME);
  return registered;
}

}  // namespace content
