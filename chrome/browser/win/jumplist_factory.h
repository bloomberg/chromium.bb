// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_
#define CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/win/jumplist.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"

class JumpListFactory : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<JumpList> GetForProfile(Profile* profile);

  static JumpListFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<JumpListFactory>;
  JumpListFactory();
  ~JumpListFactory() override;

  // BrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_WIN_JUMPLIST_FACTORY_H_
