# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

android_webview_manifest_file := $(call my-dir)/AndroidManifest.xml

# Resources.
# The res_hack folder is necessary to defeat a build system "optimization" which
# ends up skipping running aapt if there are no resource files in any of the
# resources dirs. Unfortunately, because all of our resources are generated at
# build time and because this check is performed when processing the Makefile
# it tests positive when building from clean resulting in a build failure.
# We defeat the optimization by including an empty values.xml file in the list.
android_webview_resources_dirs := \
    $(call my-dir)/res_hack \
    $(call intermediates-dir-for,GYP,shared)/android_webview_jarjar_content_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,shared)/android_webview_jarjar_ui_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,ui_strings_grd)/ui_strings_grd/res_grit \
    $(call intermediates-dir-for,GYP,content_strings_grd)/content_strings_grd/res_grit

android_webview_aapt_flags := --auto-add-overlay
android_webview_aapt_flags += --extra-packages org.chromium.ui
android_webview_aapt_flags += --extra-packages org.chromium.content
