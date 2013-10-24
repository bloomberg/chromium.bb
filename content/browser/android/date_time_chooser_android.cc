// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/date_time_chooser_android.h"

#include "base/android/jni_string.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/render_view_host.h"
#include "jni/DateTimeChooserAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;


namespace content {

// DateTimeChooserAndroid implementation
DateTimeChooserAndroid::DateTimeChooserAndroid()
  : host_(NULL) {
}

DateTimeChooserAndroid::~DateTimeChooserAndroid() {
}

// static
void DateTimeChooserAndroid::InitializeDateInputTypes(
      int text_input_type_date, int text_input_type_date_time,
      int text_input_type_date_time_local, int text_input_type_month,
      int text_input_type_time, int text_input_type_week) {
  JNIEnv* env = AttachCurrentThread();
  Java_DateTimeChooserAndroid_initializeDateInputTypes(
         env,
         text_input_type_date, text_input_type_date_time,
         text_input_type_date_time_local, text_input_type_month,
         text_input_type_time, text_input_type_week);
}

void DateTimeChooserAndroid::ReplaceDateTime(JNIEnv* env,
                                             jobject,
                                             int dialog_type,
                                             int year,
                                             int month,
                                             int day,
                                             int hour,
                                             int minute,
                                             int second,
                                             int milli,
                                             int week) {
  ViewHostMsg_DateTimeDialogValue_Params value;
  value.year = year;
  value.month = month;
  value.day = day;
  value.hour = hour;
  value.minute = minute;
  value.second = second;
  value.milli = milli;
  value.week = week;
  value.dialog_type = dialog_type;
  host_->Send(new ViewMsg_ReplaceDateTime(host_->GetRoutingID(), value));
}

void DateTimeChooserAndroid::CancelDialog(JNIEnv* env, jobject) {
  host_->Send(new ViewMsg_CancelDateTimeDialog(host_->GetRoutingID()));
}

void DateTimeChooserAndroid::ShowDialog(ContentViewCore* content,
                                        RenderViewHost* host,
                                        int type,
                                        int year,
                                        int month,
                                        int day,
                                        int hour,
                                        int minute,
                                        int second,
                                        int milli,
                                        int week,
                                        double min,
                                        double max,
                                        double step) {
  host_ = host;

  JNIEnv* env = AttachCurrentThread();
  j_date_time_chooser_.Reset(Java_DateTimeChooserAndroid_createDateTimeChooser(
      env,
      content->GetJavaObject().obj(),
      reinterpret_cast<intptr_t>(this),
      type,
      year,
      month,
      day,
      hour,
      minute,
      second,
      milli,
      week,
      min,
      max,
      step));
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
        ui::TEXT_INPUT_TYPE_TIME,
        ui::TEXT_INPUT_TYPE_WEEK);
  return registered;
}

}  // namespace content
