# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This Android makefile is used to build WebView in the Android build system.
# gyp autogenerates the real makefiles, which we just include here if we are
# doing a WebView build. For other builds, this makefile does nothing, which
# prevents the Android build system from mistakenly loading any other
# Android.mk that may exist in the Chromium tree.

ifdef CHROME_ANDROID_BUILD_WEBVIEW
include $(call my-dir)/GypAndroid.mk
endif
