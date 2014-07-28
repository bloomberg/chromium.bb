# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This makefile fragment describes how to install a Chromium pak into the
# Android framework for use by WebView.

my_local_module := $(LOCAL_MODULE)
my_local_module_stem := $(LOCAL_MODULE_STEM)
my_local_built_module_stem := $(LOCAL_BUILT_MODULE_STEM)

# This module is used to copy the .pak files to /system/webview/paks in the
# system image.
LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE_SUFFIX := .pak
LOCAL_MODULE_PATH := $(TARGET_OUT_JAVA_LIBRARIES)/webview/paks

include $(BUILD_SYSTEM)/base_rules.mk

built_by_gyp := $(call intermediates-dir-for,GYP,shared)/$(LOCAL_BUILT_MODULE_STEM)

$(eval $(call copy-one-file,$(built_by_gyp),$(LOCAL_BUILT_MODULE)))

include $(CLEAR_VARS)

# This module is used to copy the .pak files to an intermediates folder so that
# they can be picked up by AAPT and included in an .apk.
LOCAL_MODULE := res-$(my_local_module)
LOCAL_MODULE_STEM := $(my_local_module_stem)
LOCAL_BUILT_MODULE_STEM := $(my_local_built_module_stem)
LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE_SUFFIX := .pak
LOCAL_MODULE_PATH := $(call intermediates-dir-for,APPS,webviewchromium-paks)

include $(BUILD_SYSTEM)/base_rules.mk

built_by_gyp := $(call intermediates-dir-for,GYP,shared)/$(LOCAL_BUILT_MODULE_STEM)

$(eval $(call copy-one-file,$(built_by_gyp),$(LOCAL_BUILT_MODULE)))
