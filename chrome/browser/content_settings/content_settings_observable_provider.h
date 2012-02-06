// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_

#include <string>

#include "base/observer_list.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_observer.h"
#include "chrome/common/content_settings_pattern.h"

namespace content_settings {

class ObservableProvider : public ProviderInterface {
 public:
  ObservableProvider();
  virtual ~ObservableProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyObservers(const ContentSettingsPattern& primary_pattern,
                       const ContentSettingsPattern& secondary_pattern,
                       ContentSettingsType content_type,
                       const std::string& resource_identifier);
  void RemoveAllObservers();

 private:
  ObserverList<Observer, true> observer_list_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
