# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This package provides the parts of the WebView java code which live in the
# Chromium tree. This is built into a static library so it can be used by the
# glue layer in the Android tree.

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := android_webview_java

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call all-java-files-under, java/src)

# resource glue layer
LOCAL_SRC_FILES += \
    $(call all-java-files-under, ../content/public/android/java/resource_map) \
    $(call all-java-files-under, ../ui/android/java/resource_map) \

# contentview and its dependencies
LOCAL_AIDL_INCLUDES := \
    $(LOCAL_PATH)/../content/public/android/java/src \
    $(LOCAL_PATH)/../third_party/eyesfree/src/android/java/src
LOCAL_SRC_FILES += \
    $(call all-java-files-under, ../content/public/android/java/src) \
    ../content/public/android/java/src/org/chromium/content/common/ISandboxedProcessCallback.aidl \
    ../content/public/android/java/src/org/chromium/content/common/ISandboxedProcessService.aidl \
    $(call all-java-files-under, ../base/android/java/src) \
    $(call all-java-files-under, ../media/base/android/java/src) \
    $(call all-java-files-under, ../net/android/java/src) \
    $(call all-java-files-under, ../ui/android/java/src) \
    $(call all-java-files-under, ../third_party/eyesfree/src/android/java/src) \
    $(call all-Iaidl-files-under, ../third_party/eyesfree/src/android/java/src)

# browser components
LOCAL_SRC_FILES += \
    $(call all-java-files-under, \
        ../components/web_contents_delegate_android/android/java/src) \
    $(call all-java-files-under, \
        ../components/navigation_interception/android/java/src) \


# Java files generated from .template rules. This list should match list of java dependencies in
# android_webview/all_webview.gyp
LOCAL_GENERATED_SOURCES := \
$(call intermediates-dir-for,GYP,shared)/templates/org.chromium.content.browser/PageTransitionTypes.java \
$(call intermediates-dir-for,GYP,shared)/templates/org.chromium.net/CertificateMimeType.java \
$(call intermediates-dir-for,GYP,shared)/templates/org.chromium.net/CertVerifyResultAndroid.java \
$(call intermediates-dir-for,GYP,shared)/templates/org.chromium.net/NetError.java \
$(call intermediates-dir-for,GYP,shared)/templates/org.chromium.net/PrivateKeyType.java \

# content dependencies on java components that are provided by the system on
# android
LOCAL_STATIC_JAVA_LIBRARIES += jsr305 guava

include $(BUILD_STATIC_JAVA_LIBRARY)


########################################################
# These packages are the resource paks used by webview.

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_pak
LOCAL_MODULE_STEM := webviewchromium
LOCAL_BUILT_MODULE_STEM := android_webview_apk/assets/webviewchromium.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_am.pak
LOCAL_MODULE_STEM := am
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_am.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ar.pak
LOCAL_MODULE_STEM := ar
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ar.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_bg.pak
LOCAL_MODULE_STEM := bg
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_bg.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_bn.pak
LOCAL_MODULE_STEM := bn
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_bn.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ca.pak
LOCAL_MODULE_STEM := ca
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ca.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_cs.pak
LOCAL_MODULE_STEM := cs
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_cs.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_da.pak
LOCAL_MODULE_STEM := da
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_da.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_de.pak
LOCAL_MODULE_STEM := de
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_de.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_el.pak
LOCAL_MODULE_STEM := el
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_el.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_en-GB.pak
LOCAL_MODULE_STEM := en-GB
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_en-GB.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_en-US.pak
LOCAL_MODULE_STEM := en-US
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_en-US.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_es-419.pak
LOCAL_MODULE_STEM := es-419
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_es-419.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_es.pak
LOCAL_MODULE_STEM := es
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_es.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_et.pak
LOCAL_MODULE_STEM := et
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_et.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_fa.pak
LOCAL_MODULE_STEM := fa
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_fa.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_fil.pak
LOCAL_MODULE_STEM := fil
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_fil.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_fi.pak
LOCAL_MODULE_STEM := fi
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_fi.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_fr.pak
LOCAL_MODULE_STEM := fr
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_fr.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_gu.pak
LOCAL_MODULE_STEM := gu
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_gu.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_he.pak
LOCAL_MODULE_STEM := he
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_he.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_hi.pak
LOCAL_MODULE_STEM := hi
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_hi.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_hr.pak
LOCAL_MODULE_STEM := hr
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_hr.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_hu.pak
LOCAL_MODULE_STEM := hu
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_hu.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_id.pak
LOCAL_MODULE_STEM := id
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_id.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_it.pak
LOCAL_MODULE_STEM := it
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_it.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ja.pak
LOCAL_MODULE_STEM := ja
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ja.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_kn.pak
LOCAL_MODULE_STEM := kn
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_kn.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ko.pak
LOCAL_MODULE_STEM := ko
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ko.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_lt.pak
LOCAL_MODULE_STEM := lt
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_lt.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_lv.pak
LOCAL_MODULE_STEM := lv
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_lv.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ml.pak
LOCAL_MODULE_STEM := ml
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ml.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_mr.pak
LOCAL_MODULE_STEM := mr
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_mr.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ms.pak
LOCAL_MODULE_STEM := ms
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ms.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_nb.pak
LOCAL_MODULE_STEM := nb
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_nb.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_nl.pak
LOCAL_MODULE_STEM := nl
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_nl.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_pl.pak
LOCAL_MODULE_STEM := pl
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_pl.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_pt-BR.pak
LOCAL_MODULE_STEM := pt-BR
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_pt-BR.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_pt-PT.pak
LOCAL_MODULE_STEM := pt-PT
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_pt-PT.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ro.pak
LOCAL_MODULE_STEM := ro
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ro.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ru.pak
LOCAL_MODULE_STEM := ru
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ru.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_sk.pak
LOCAL_MODULE_STEM := sk
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_sk.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_sl.pak
LOCAL_MODULE_STEM := sl
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_sl.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_sr.pak
LOCAL_MODULE_STEM := sr
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_sr.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_sv.pak
LOCAL_MODULE_STEM := sv
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_sv.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_sw.pak
LOCAL_MODULE_STEM := sw
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_sw.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_ta.pak
LOCAL_MODULE_STEM := ta
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_ta.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_te.pak
LOCAL_MODULE_STEM := te
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_te.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_th.pak
LOCAL_MODULE_STEM := th
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_th.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_tr.pak
LOCAL_MODULE_STEM := tr
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_tr.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_uk.pak
LOCAL_MODULE_STEM := uk
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_uk.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_vi.pak
LOCAL_MODULE_STEM := vi
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_vi.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_zh-CN.pak
LOCAL_MODULE_STEM := zh-CN
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_zh-CN.pak
include $(LOCAL_PATH)/webview_pak.mk

include $(CLEAR_VARS)
LOCAL_MODULE := webviewchromium_webkit_strings_zh-TW.pak
LOCAL_MODULE_STEM := zh-TW
LOCAL_BUILT_MODULE_STEM := webkit/webkit_strings_zh-TW.pak
include $(LOCAL_PATH)/webview_pak.mk
