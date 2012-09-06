// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEB_INTENT_CALLBACKS_H_
#define CHROME_BROWSER_EXTENSIONS_WEB_INTENT_CALLBACKS_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace content {
class WebContents;
class WebIntentsDispatcher;
}

namespace extensions {
class Extension;

// The WebIntentCallbacks class tracks the pending callbacks for web intents
// that have been delivered to packaged apps (i.e., new-style apps containing
// shell windows), for a particular profile.
class WebIntentCallbacks : public ProfileKeyedService {
 public:
  WebIntentCallbacks();
  virtual ~WebIntentCallbacks();

  // Returns the instance for the given profile. This is a convenience wrapper
  // around WebIntentCallbacks::Factory::GetForProfile.
  static WebIntentCallbacks* Get(Profile* profile);

  // Registers the callback for the Web Intent we're about to dispatch to the
  // given extension. Returns an identifier that should later be dispatched to
  // RetrieveCallback in order to invoke the callback registered here.
  // This transfers ownership of WebIntentsDispatcher to this class.
  int RegisterCallback(const Extension* extension,
                       content::WebIntentsDispatcher* dispatcher,
                       content::WebContents* source);

  // Retrieves the callback for the given identifier, for a response from the
  // specified extension. This will clear the callback. If there is no callback
  // registered under this identifier, then this will return NULL. This
  // transfers ownership of WebIntentsDispatcher to the caller.
  content::WebIntentsDispatcher* RetrieveCallback(const Extension* extension,
                                                  int intent_id);

 private:
  typedef std::map<std::string, content::WebIntentsDispatcher*> CallbackMap;

  class Factory : public ProfileKeyedServiceFactory {
   public:
    static WebIntentCallbacks* GetForProfile(Profile* profile);

    static Factory* GetInstance();
   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // ProfileKeyedServiceFactory
    virtual ProfileKeyedService* BuildServiceInstanceFor(
        Profile* profile) const OVERRIDE;
  };

  // Private method to get and clear the dispatcher for the given string key.
  // If there is no dispatcher available, this will return NULL. Otherwise, this
  // transfers ownership of the WebIntentsDispatcher to the caller.
  content::WebIntentsDispatcher* GetAndClear(const std::string& key);

  class SourceObserver;

  // Used as an incrementing ID for callback keys.
  int last_id_;

  // Stores all pending callbacks sent to platform apps.
  CallbackMap pending_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEB_INTENT_CALLBACKS_H_
