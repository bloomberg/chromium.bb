// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_pdf_exporter.h"

#include "android_webview/browser/renderer_host/print_manager.h"
#include "base/android/jni_android.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/AwPdfExporter_jni.h"
#include "printing/print_settings.h"
#include "printing/units.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;
using content::WebContents;
using printing::ConvertUnitDouble;
using printing::PageMargins;
using printing::PrintSettings;

namespace android_webview {

AwPdfExporter::AwPdfExporter(JNIEnv* env,
                             jobject obj,
                             WebContents* web_contents)
    : java_ref_(env, obj),
      web_contents_(web_contents) {
  DCHECK(obj);
  Java_AwPdfExporter_setNativeAwPdfExporter(
      env, obj, reinterpret_cast<intptr_t>(this));
}

AwPdfExporter::~AwPdfExporter() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  // Clear the Java peer's weak pointer to |this| object.
  Java_AwPdfExporter_setNativeAwPdfExporter(env, obj.obj(), 0);
}

void AwPdfExporter::ExportToPdf(JNIEnv* env,
                                jobject obj,
                                int fd,
                                jobject cancel_signal) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CreatePdfSettings(env, obj);
  print_manager_.reset(
      new PrintManager(web_contents_, print_settings_.get(), fd, this));
  if (!print_manager_->PrintNow())
    DidExportPdf(false);
}

namespace {
// Converts from 1/1000 of inches to device units using DPI.
int MilsToDots(int val, int dpi) {
  return static_cast<int>(ConvertUnitDouble(val, 1000.0, dpi));
}

}  // anonymous namespace

void AwPdfExporter::CreatePdfSettings(JNIEnv* env, jobject obj) {
  print_settings_.reset(new PrintSettings);
  int dpi = Java_AwPdfExporter_getDpi(env, obj);
  int width = Java_AwPdfExporter_getPageWidth(env, obj);
  int height = Java_AwPdfExporter_getPageHeight(env, obj);
  gfx::Size physical_size_device_units;
  int width_in_dots = MilsToDots(width, dpi);
  int height_in_dots = MilsToDots(height, dpi);
  physical_size_device_units.SetSize(width_in_dots, height_in_dots);

  gfx::Rect printable_area_device_units;
  // Assume full page is printable for now.
  printable_area_device_units.SetRect(0, 0, width_in_dots, height_in_dots);

  print_settings_->set_dpi(dpi);
  // TODO(sgurun) verify that the value for newly added parameter for
  // (i.e. landscape_needs_flip) is correct.
  print_settings_->SetPrinterPrintableArea(physical_size_device_units,
                                           printable_area_device_units,
                                           true);

  PageMargins margins;
  margins.left =
      MilsToDots(Java_AwPdfExporter_getLeftMargin(env, obj), dpi);
  margins.right =
      MilsToDots(Java_AwPdfExporter_getRightMargin(env, obj), dpi);
  margins.top =
      MilsToDots(Java_AwPdfExporter_getTopMargin(env, obj), dpi);
  margins.bottom =
      MilsToDots(Java_AwPdfExporter_getBottomMargin(env, obj), dpi);
  print_settings_->SetCustomMargins(margins);
  print_settings_->set_should_print_backgrounds(true);
}

void AwPdfExporter::DidExportPdf(bool success) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwPdfExporter_didExportPdf(env, obj.obj(), success);
}

bool AwPdfExporter::IsCancelled() {
  // TODO(sgurun) implement. Needs connecting with the |cancel_signal| passed
  // in the constructor.
  return false;
}

bool RegisterAwPdfExporter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
