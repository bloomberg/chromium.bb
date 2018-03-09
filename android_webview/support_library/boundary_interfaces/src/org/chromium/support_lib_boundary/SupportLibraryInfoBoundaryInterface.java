// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

/**
 * Contains information about the WebView support library side, e.g. which features are supported on
 * that side.
 * This is passed to the WebView APK code on support library initialization.
 */
public interface SupportLibraryInfoBoundaryInterface { String[] getSupportedFeatures(); }
