// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_pdf_exporter.h"

#include "android_webview/browser/aw_print_manager.h"
#include "base/android/jni_android.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AwPdfExporter_jni.h"
#include "printing/print_settings.h"
#include "printing/units.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android_webview {

AwPdfExporter::AwPdfExporter(JNIEnv* env,
                             const JavaRef<jobject>& obj,
                             content::WebContents* web_contents)
    : java_ref_(env, obj), web_contents_(web_contents) {
  DCHECK(!obj.is_null());
  Java_AwPdfExporter_setNativeAwPdfExporter(
      env, obj, reinterpret_cast<intptr_t>(this));
}

AwPdfExporter::~AwPdfExporter() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  // Clear the Java peer's weak pointer to |this| object.
  Java_AwPdfExporter_setNativeAwPdfExporter(env, obj, 0);
}

void AwPdfExporter::ExportToPdf(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                int fd,
                                const JavaParamRef<jobject>& cancel_signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  printing::PrintSettings print_settings;
  InitPdfSettings(env, obj, print_settings);
  AwPrintManager* print_manager =
      AwPrintManager::CreateForWebContents(
          web_contents_, print_settings, base::FileDescriptor(fd, false),
          base::Bind(&AwPdfExporter::DidExportPdf, base::Unretained(this)));

  if (!print_manager->PrintNow())
    DidExportPdf(fd, false);
}

namespace {
// Converts from 1/1000 of inches to device units using DPI.
int MilsToDots(int val, int dpi) {
  return static_cast<int>(printing::ConvertUnitDouble(val, 1000.0, dpi));
}
}  // anonymous namespace

void AwPdfExporter::InitPdfSettings(JNIEnv* env,
                                    const JavaRef<jobject>& obj,
                                    printing::PrintSettings& settings) {
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

  settings.set_dpi(dpi);
  // TODO(sgurun) verify that the value for newly added parameter for
  // (i.e. landscape_needs_flip) is correct.
  settings.SetPrinterPrintableArea(physical_size_device_units,
                                   printable_area_device_units,
                                   true);

  printing::PageMargins margins;
  margins.left =
      MilsToDots(Java_AwPdfExporter_getLeftMargin(env, obj), dpi);
  margins.right =
      MilsToDots(Java_AwPdfExporter_getRightMargin(env, obj), dpi);
  margins.top =
      MilsToDots(Java_AwPdfExporter_getTopMargin(env, obj), dpi);
  margins.bottom =
      MilsToDots(Java_AwPdfExporter_getBottomMargin(env, obj), dpi);
  settings.SetCustomMargins(margins);
  settings.set_should_print_backgrounds(true);
}

void AwPdfExporter::DidExportPdf(int fd, bool success) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_AwPdfExporter_didExportPdf(env, obj, success);
}

bool RegisterAwPdfExporter(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
