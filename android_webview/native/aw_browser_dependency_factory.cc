// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_browser_dependency_factory.h"

#include "base/logging.h"

namespace android_webview {

namespace {

AwBrowserDependencyFactory* g_instance = NULL;

}  // namespace

AwBrowserDependencyFactory::AwBrowserDependencyFactory() {}

AwBrowserDependencyFactory::~AwBrowserDependencyFactory() {}

// static
void AwBrowserDependencyFactory::SetInstance(
    AwBrowserDependencyFactory* delegate) {
  g_instance = delegate;
}

// static
AwBrowserDependencyFactory* AwBrowserDependencyFactory::GetInstance() {
  DCHECK(g_instance);  // Must always be confirgured on startup.
  return g_instance;
}

}  // namespace android_webview

