// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_

#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class UserEventService;
}

class PrefService;
class PrefRegistrySimple;

namespace consent_auditor {

class ConsentAuditor : public KeyedService {
 public:
  ConsentAuditor(PrefService* pref_service,
                 syncer::UserEventService* user_event_service,
                 const std::string& app_version,
                 const std::string& app_locale);
  ~ConsentAuditor() override;

  // KeyedService:
  void Shutdown() override;

  // Registers the preferences needed by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Records that the user consented to a |feature|. The user was presented with
  // |description_text| and accepted it by interacting |confirmation_text|
  // (e.g. clicking on a button; empty if not applicable).
  // Returns true if successful.
  void RecordLocalConsent(const std::string& feature,
                          const std::string& description_text,
                          const std::string& confirmation_text);

 private:
  PrefService* pref_service_;
  syncer::UserEventService* user_event_service_;
  std::string app_version_;
  std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(ConsentAuditor);
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
