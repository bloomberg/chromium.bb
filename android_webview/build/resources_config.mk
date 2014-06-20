# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

android_webview_manifest_file := $(call my-dir)/AndroidManifest.xml

# resources
android_webview_resources_dirs := \
    $(call intermediates-dir-for,GYP,shared,,,$(TARGET_2ND_ARCH))/android_webview_jarjar_content_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,shared,,,$(TARGET_2ND_ARCH))/android_webview_jarjar_ui_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,ui_strings_grd,,,$(TARGET_2ND_ARCH))/ui_strings_grd/res_grit \
    $(call intermediates-dir-for,GYP,content_strings_grd,,,$(TARGET_2ND_ARCH))/content_strings_grd/res_grit

android_webview_aapt_flags := --auto-add-overlay
android_webview_aapt_flags += --custom-package com.android.webview.chromium
android_webview_aapt_flags += --extra-packages org.chromium.ui
android_webview_aapt_flags += --extra-packages org.chromium.content
