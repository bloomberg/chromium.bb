// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_

#include <string>

#include "base/singleton.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class PrefService;
class TabContents;

// The TranslateManager class is responsible for showing an info-bar when a page
// in a language different than the user language is loaded.  It triggers the
// page translation the user requests.
// It is a singleton.

class TranslateManager : public NotificationObserver {
 public:
  virtual ~TranslateManager();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 private:
  friend struct DefaultSingletonTraits<TranslateManager>;

  TranslateManager();

  // Starts the translation process on |tab| containing the page in the
  // |page_lang| language.
  void InitiateTranslation(TabContents* tab, const std::string& page_lang);

  // Convenience method that retrieves the PrefService.
  PrefService* GetPrefService(TabContents* tab);

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TranslateManager);
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_MANAGER_H_
