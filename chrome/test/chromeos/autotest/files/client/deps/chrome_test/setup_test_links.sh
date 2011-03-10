#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A script to setup symbolic links needed for Chrome's automated UI tests.

ln -f -s /opt/google/chrome/chrome $(dirname $0)/chrome
ln -f -s /opt/google/chrome/chrome.pak $(dirname $0)/chrome.pak
ln -f -s /opt/google/chrome/locales $(dirname $0)/locales
ln -f -s /opt/google/chrome/resources $(dirname $0)/resources
ln -f -s /opt/google/chrome/resources.pak $(dirname $0)/resources.pak

mkdir -p $(dirname $0)/chromeos
ln -f -s /opt/google/chrome/chromeos/libcros.so \
    $(dirname $0)/chromeos/libcros.so
