# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This makefile fragment describes how to install a Chromium pak into the
# Android framework for use by WebView.

LOCAL_MODULE_CLASS := GYP
LOCAL_MODULE_SUFFIX := .pak
LOCAL_MODULE_PATH := $(TARGET_OUT_JAVA_LIBRARIES)/webview/paks

include $(BUILD_SYSTEM)/base_rules.mk

# TODO(torne): remove TARGET_2ND_ARCH here once we're no longer 64-bit blacklisted in the Android
# build system. http://crbug.com/358141
built_by_gyp := $(call intermediates-dir-for,GYP,shared,,,$(TARGET_2ND_ARCH))/$(LOCAL_BUILT_MODULE_STEM)

$(eval $(call copy-one-file,$(built_by_gyp),$(LOCAL_BUILT_MODULE)))
