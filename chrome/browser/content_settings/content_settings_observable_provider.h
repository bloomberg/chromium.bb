// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_

#include <set>
#include <string>

#include "base/observer_list.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/content_settings_observer.h"

namespace content_settings {

class ObservableDefaultProvider : public DefaultProviderInterface {
 public:
  ObservableDefaultProvider();
  virtual ~ObservableDefaultProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyObservers(ContentSettingsPattern primary_pattern,
                       ContentSettingsPattern secondary_pattern,
                       ContentSettingsType content_type,
                       std::string resource_identifier);
  void RemoveAllObservers();

 private:
  ObserverList<Observer, true> observer_list_;
};

class ObservableProvider : public ProviderInterface {
 public:
  ObservableProvider();
  virtual ~ObservableProvider();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyObservers(ContentSettingsPattern primary_pattern,
                       ContentSettingsPattern secondary_pattern,
                       ContentSettingsType content_type,
                       std::string resource_identifier);
  void RemoveAllObservers();

 private:
  ObserverList<Observer, true> observer_list_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CONTENT_SETTINGS_OBSERVABLE_PROVIDER_H_
