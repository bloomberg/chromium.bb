// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host_manager.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

namespace content {

JavaBridgeDispatcherHostManager::JavaBridgeDispatcherHostManager(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

JavaBridgeDispatcherHostManager::~JavaBridgeDispatcherHostManager() {
  for (ObjectMap::iterator iter = objects_.begin(); iter != objects_.end();
      ++iter) {
    WebKit::WebBindings::releaseObject(iter->second);
  }
  DCHECK_EQ(0U, instances_.size());
}

void JavaBridgeDispatcherHostManager::AddNamedObject(const string16& name,
    NPObject* object) {
  // Record this object in a map so that we can add it into RenderViewHosts
  // created later. The JavaBridgeDispatcherHost instances will take a
  // reference to the object, but we take one too, because this method can be
  // called before there are any such instances.
  WebKit::WebBindings::retainObject(object);
  objects_[name] = object;

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->AddNamedObject(name, object);
  }
}

void JavaBridgeDispatcherHostManager::RemoveNamedObject(const string16& name) {
  ObjectMap::iterator iter = objects_.find(name);
  if (iter == objects_.end()) {
    return;
  }

  WebKit::WebBindings::releaseObject(iter->second);
  objects_.erase(iter);

  for (InstanceMap::iterator iter = instances_.begin();
      iter != instances_.end(); ++iter) {
    iter->second->RemoveNamedObject(name);
  }
}

void JavaBridgeDispatcherHostManager::RenderViewCreated(
    RenderViewHost* render_view_host) {
  // Creates a JavaBridgeDispatcherHost for the specified RenderViewHost and
  // adds all currently registered named objects to the new instance.
  scoped_refptr<JavaBridgeDispatcherHost> instance =
      new JavaBridgeDispatcherHost(render_view_host);

  for (ObjectMap::const_iterator iter = objects_.begin();
      iter != objects_.end(); ++iter) {
    instance->AddNamedObject(iter->first, iter->second);
  }

  instances_[render_view_host] = instance;
}

void JavaBridgeDispatcherHostManager::RenderViewDeleted(
    RenderViewHost* render_view_host) {
  instances_.erase(render_view_host);
}

void JavaBridgeDispatcherHostManager::WebContentsDestroyed(
    WebContents* web_contents) {
  // When a WebContents is shutting down, it clears its observers before
  // it kills all of its RenderViewHosts, so we won't get a call to
  // RenderViewDeleted() for all RenderViewHosts.
  instances_.clear();
}

}  // namespace content
