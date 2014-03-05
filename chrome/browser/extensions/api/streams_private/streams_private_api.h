// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STREAMS_PRIVATE_STREAMS_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_STREAMS_PRIVATE_STREAMS_PRIVATE_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/values.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"

class Profile;

namespace content {
class BrowserContext;
class StreamHandle;
}

namespace extensions {

class StreamsPrivateAPI : public BrowserContextKeyedAPI,
                          public content::NotificationObserver {
 public:
  // Convenience method to get the StreamsPrivateAPI for a profile.
  static StreamsPrivateAPI* Get(content::BrowserContext* context);

  explicit StreamsPrivateAPI(content::BrowserContext* context);
  virtual ~StreamsPrivateAPI();

  void ExecuteMimeTypeHandler(const std::string& extension_id,
                              const content::WebContents* web_contents,
                              scoped_ptr<content::StreamHandle> stream,
                              int64 expected_content_size);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<StreamsPrivateAPI>* GetFactoryInstance();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class BrowserContextKeyedAPIFactory<StreamsPrivateAPI>;
  typedef std::map<std::string,
                   std::map<GURL,
                            linked_ptr<content::StreamHandle> > > StreamMap;

  // BrowserContextKeyedAPI implementation.
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
