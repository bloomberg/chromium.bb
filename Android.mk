# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Android makefile is used to build WebView in the Android build system.
# gyp autogenerates most of the real makefiles, which we include below.

# Don't do anything if the product is using a prebuilt webviewchromium, to avoid
# duplicate target definitions between this directory and the prebuilts.
ifneq ($(PRODUCT_PREBUILT_WEBVIEWCHROMIUM),yes)

CHROMIUM_DIR := $(call my-dir)

# We default to release for the Android build system. Developers working on
# WebView code can build with "make GYP_CONFIGURATION=Debug".
GYP_CONFIGURATION := Release

# Include the manually-written makefile that builds all the WebView java code.
include $(CHROMIUM_DIR)/android_webview/Android.mk

# If the gyp-generated makefile exists for the current host OS and primary
# target architecture, we need to include it. If it doesn't exist then just do
# nothing, since we may not have finished bringing up this architecture yet.
# We set GYP_VAR_PREFIX to the empty string to indicate that we are building for
# the primary target architecture.
ifneq (,$(wildcard $(CHROMIUM_DIR)/GypAndroid.$(HOST_OS)-$(TARGET_ARCH).mk))
GYP_VAR_PREFIX :=
include $(CHROMIUM_DIR)/GypAndroid.$(HOST_OS)-$(TARGET_ARCH).mk
endif

# Do the same check for the secondary architecture; if this doesn't exist then
# the current target platform probably doesn't have a secondary architecture and
# we can just do nothing.
# We set GYP_VAR_PREFIX to $(TARGET_2ND_ARCH_VAR_PREFIX) to indicate that we are
# building for the secondary target architecture.
ifneq (,$(wildcard $(CHROMIUM_DIR)/GypAndroid.$(HOST_OS)-$(TARGET_2ND_ARCH).mk))
GYP_VAR_PREFIX := $(TARGET_2ND_ARCH_VAR_PREFIX)
include $(CHROMIUM_DIR)/GypAndroid.$(HOST_OS)-$(TARGET_2ND_ARCH).mk
endif

endif
