// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "chrome/browser/extensions/web_intent_callbacks.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_intents_dispatcher.h"

namespace {

// Get the key for use inside the internal pending WebIntentCallbacks map.
std::string GetKey(const extensions::Extension* extension, int id) {
  return StringPrintf("%s/%d", extension->id().c_str(), id);
}

}  // namespace

namespace extensions {

// SourceObserver is a subclass of WebContentsObserver that is instantiated
// on RegisterCallback to wait for the source WebContents to be destroyed.
// If it is destroyed, this automatically clears the callback from pending_.
class WebIntentCallbacks::SourceObserver : content::WebContentsObserver {
 public:
  SourceObserver(content::WebContents* web_contents,
                 WebIntentCallbacks* callbacks,
                 const std::string& key)
      : content::WebContentsObserver(web_contents),
        callbacks_(callbacks),
        key_(key) {}
  virtual ~SourceObserver() {}

  // Implement WebContentsObserver
  virtual void WebContentsDestroyed(content::WebContents* web_contents)
      OVERRIDE {
    content::WebIntentsDispatcher* dispatcher =
        callbacks_->GetAndClear(key_);
    if (dispatcher)
      delete dispatcher;

    delete this;
  }

 private:
  WebIntentCallbacks* callbacks_;
  const std::string key_;
};

WebIntentCallbacks::WebIntentCallbacks() : last_id_(0) {}

WebIntentCallbacks::~WebIntentCallbacks() {}

// static
WebIntentCallbacks* WebIntentCallbacks::Get(Profile* profile) {
  return Factory::GetForProfile(profile);
}

int WebIntentCallbacks::RegisterCallback(
    const Extension* extension,
    content::WebIntentsDispatcher* dispatcher,
    content::WebContents* source) {
  int id = ++last_id_;
  std::string key = GetKey(extension, id);
  pending_[key] = dispatcher;

  // TODO(thorogood): We should also listen for extension suspend events, and
  // clear the relevant callback (i.e., the handler of this intent)

  // TODO(thorogood): The source window may actually be a ShellWindow of a
  // platform app. In this case, we should not wait for its destruction:
  // rather, we should be listening to platform app suspend events.

  // Listen to when the source WebContent is destroyed. This object is self-
  // deleting on this event.
  DCHECK(source);
  new SourceObserver(source, this, key);

  return id;
}

content::WebIntentsDispatcher* WebIntentCallbacks::RetrieveCallback(
    const Extension* extension, int id) {
  std::string key = GetKey(extension, id);
  return GetAndClear(key);
}

content::WebIntentsDispatcher* WebIntentCallbacks::GetAndClear(
    const std::string& key) {
  if (!pending_.count(key))
    return NULL;

  content::WebIntentsDispatcher* dispatcher = pending_[key];
  pending_.erase(key);
  return dispatcher;
}

///////////////////////////////////////////////////////////////////////////////
// Factory boilerplate

// static
WebIntentCallbacks* WebIntentCallbacks::Factory::GetForProfile(
    Profile* profile) {
  return static_cast<WebIntentCallbacks*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

WebIntentCallbacks::Factory* WebIntentCallbacks::Factory::GetInstance() {
  return Singleton<WebIntentCallbacks::Factory>::get();
}

WebIntentCallbacks::Factory::Factory()
    : ProfileKeyedServiceFactory("WebIntentCallbacks",
                                 ProfileDependencyManager::GetInstance()) {
}

WebIntentCallbacks::Factory::~Factory() {
}

ProfileKeyedService* WebIntentCallbacks::Factory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new WebIntentCallbacks();
}

}  // namespace extensions
