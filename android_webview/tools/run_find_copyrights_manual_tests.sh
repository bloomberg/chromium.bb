#!/bin/sh
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
find android_webview/tools/tests -type f | sort \
    | android_webview/tools/webview_licenses.py display_copyrights
