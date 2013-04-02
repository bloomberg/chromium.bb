// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STREAMS_PRIVATE_STREAMS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_STREAMS_PRIVATE_STREAMS_PRIVATE_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class StreamHandle;
}

namespace extensions {

class StreamsPrivateAPI : public ProfileKeyedAPI,
                          public content::NotificationObserver {
 public:
  // Convenience method to get the StreamsPrivateAPI for a profile.
  static StreamsPrivateAPI* Get(Profile* profile);

  explicit StreamsPrivateAPI(Profile* profile);
  virtual ~StreamsPrivateAPI();

  void ExecuteMimeTypeHandler(const std::string& extension_id,
                              scoped_ptr<content::StreamHandle> stream);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<StreamsPrivateAPI>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<StreamsPrivateAPI>;
  typedef std::map<std::string,
                   std::map<GURL,
                            linked_ptr<content::StreamHandle> > > StreamMap;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "StreamsPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  Profile* const profile_;
  content::NotificationRegistrar registrar_;
  StreamMap streams_;
  base::WeakPtrFactory<StreamsPrivateAPI> weak_ptr_factory_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STREAMS_PRIVATE_STREAMS_PRIVATE_API_H_
