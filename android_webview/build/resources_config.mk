# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file defines resources-related build variables which are shared by the
# android_webview/Android.mk file and the frameworks/webview/chromium/Android.mk
# file.

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
    $(call my-dir)/../java/res \
    $(call intermediates-dir-for,GYP,shared)/android_webview_jarjar_content_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,shared)/android_webview_jarjar_ui_resources/jarjar_res \
    $(call intermediates-dir-for,GYP,ui_strings_grd)/ui_strings_grd/res_grit \
    $(call intermediates-dir-for,GYP,content_strings_grd)/content_strings_grd/res_grit \
    $(call intermediates-dir-for,GYP,android_webview_strings_grd)/android_webview_strings_grd/res_grit

android_webview_asset_dirs := \
    $(call intermediates-dir-for,APPS,webviewchromium-paks)

android_webview_aapt_flags := --auto-add-overlay
android_webview_aapt_flags += --extra-packages org.chromium.ui
android_webview_aapt_flags += --extra-packages org.chromium.content
android_webview_aapt_flags += -0 pak

android_webview_system_pak_targets := \
        webviewchromium_pak \
        webviewchromium_webkit_strings_am.pak \
        webviewchromium_webkit_strings_ar.pak \
        webviewchromium_webkit_strings_bg.pak \
        webviewchromium_webkit_strings_bn.pak \
        webviewchromium_webkit_strings_ca.pak \
        webviewchromium_webkit_strings_cs.pak \
        webviewchromium_webkit_strings_da.pak \
        webviewchromium_webkit_strings_de.pak \
        webviewchromium_webkit_strings_el.pak \
        webviewchromium_webkit_strings_en-GB.pak \
        webviewchromium_webkit_strings_en-US.pak \
        webviewchromium_webkit_strings_es-419.pak \
        webviewchromium_webkit_strings_es.pak \
        webviewchromium_webkit_strings_et.pak \
        webviewchromium_webkit_strings_fa.pak \
        webviewchromium_webkit_strings_fil.pak \
        webviewchromium_webkit_strings_fi.pak \
        webviewchromium_webkit_strings_fr.pak \
        webviewchromium_webkit_strings_gu.pak \
        webviewchromium_webkit_strings_he.pak \
        webviewchromium_webkit_strings_hi.pak \
        webviewchromium_webkit_strings_hr.pak \
        webviewchromium_webkit_strings_hu.pak \
        webviewchromium_webkit_strings_id.pak \
        webviewchromium_webkit_strings_it.pak \
        webviewchromium_webkit_strings_ja.pak \
        webviewchromium_webkit_strings_kn.pak \
        webviewchromium_webkit_strings_ko.pak \
        webviewchromium_webkit_strings_lt.pak \
        webviewchromium_webkit_strings_lv.pak \
        webviewchromium_webkit_strings_ml.pak \
        webviewchromium_webkit_strings_mr.pak \
        webviewchromium_webkit_strings_ms.pak \
        webviewchromium_webkit_strings_nb.pak \
        webviewchromium_webkit_strings_nl.pak \
        webviewchromium_webkit_strings_pl.pak \
        webviewchromium_webkit_strings_pt-BR.pak \
        webviewchromium_webkit_strings_pt-PT.pak \
        webviewchromium_webkit_strings_ro.pak \
        webviewchromium_webkit_strings_ru.pak \
        webviewchromium_webkit_strings_sk.pak \
        webviewchromium_webkit_strings_sl.pak \
        webviewchromium_webkit_strings_sr.pak \
        webviewchromium_webkit_strings_sv.pak \
        webviewchromium_webkit_strings_sw.pak \
        webviewchromium_webkit_strings_ta.pak \
        webviewchromium_webkit_strings_te.pak \
        webviewchromium_webkit_strings_th.pak \
        webviewchromium_webkit_strings_tr.pak \
        webviewchromium_webkit_strings_uk.pak \
        webviewchromium_webkit_strings_vi.pak \
        webviewchromium_webkit_strings_zh-CN.pak \
        webviewchromium_webkit_strings_zh-TW.pak

android_webview_final_pak_names := \
  $(patsubst webviewchromium_pak,webviewchromium.pak, \
    $(patsubst webviewchromium_webkit_strings_%,%, \
      $(android_webview_system_pak_targets)))

# This list will be used to force the .pak files to be copied into the
# intermediates folder before invoking appt from the Android 'glue layer'
# makefile.
android_webview_intermediates_pak_additional_deps := \
  $(foreach name,$(android_webview_final_pak_names), \
    $(call intermediates-dir-for,APPS,webviewchromium-paks)/$(name))

# This is the stamp file for the android_webview_resources target.
android_webview_resources_stamp := \
  $(call intermediates-dir-for,GYP,android_webview_resources)/android_webview_resources.stamp
