// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_url_loader.h"

#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/navigation_url_loader_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"

namespace content {

static NavigationURLLoaderFactory* g_factory = nullptr;

scoped_ptr<NavigationURLLoader> NavigationURLLoader::Create(
    BrowserContext* browser_context,
    int64 frame_tree_node_id,
    scoped_ptr<NavigationRequestInfo> request_info,
    NavigationURLLoaderDelegate* delegate) {
  if (g_factory) {
    return g_factory->CreateLoader(browser_context, frame_tree_node_id,
                                   request_info.Pass(), delegate);
  }
  return scoped_ptr<NavigationURLLoader>(new NavigationURLLoaderImpl(
      browser_context, frame_tree_node_id, request_info.Pass(), delegate));
}

void NavigationURLLoader::SetFactoryForTesting(
    NavigationURLLoaderFactory* factory) {
  DCHECK(g_factory == nullptr || factory == nullptr);
  g_factory = factory;
}

}  // namespace content
