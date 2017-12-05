// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
#define COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace syncer {
class UserEventService;
}

namespace sync_pb {
class UserEventSpecifics;
}

class PrefService;
class PrefRegistrySimple;

namespace consent_auditor {

class ConsentAuditor : public KeyedService {
 public:
  enum class ConsentStatus { REVOKED, GIVEN };

  ConsentAuditor(PrefService* pref_service,
                 syncer::UserEventService* user_event_service,
                 const std::string& app_version,
                 const std::string& app_locale);
  ~ConsentAuditor() override;

  // KeyedService:
  void Shutdown() override;

  // Registers the preferences needed by this service.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Records a consent for |feature| for the signed-in GAIA account.
  // Descriptions and button texts presented to the user were
  // |consent_grd_ids|. Strings put into placeholders are passed as
  // |placeholder_replacements|.
  // Whether the consent was GIVEN or REVOKED is passed as |status|.
  void RecordGaiaConsent(
      const std::string& feature,
      const std::vector<int>& consent_grd_ids,
      const std::vector<std::string>& placeholder_replacements,
      ConsentStatus status);

  // Records that the user consented to a |feature|. The user was presented with
  // |description_text| and accepted it by interacting |confirmation_text|
  // (e.g. clicking on a button; empty if not applicable).
  // Returns true if successful.
  void RecordLocalConsent(const std::string& feature,
                          const std::string& description_text,
                          const std::string& confirmation_text);

 private:
  std::unique_ptr<sync_pb::UserEventSpecifics> ConstructUserConsent(
      const std::string& feature,
      const std::vector<int>& consent_grd_ids,
      const std::vector<std::string>& placeholder_replacements,
      ConsentAuditor::ConsentStatus status);

  PrefService* pref_service_;
  syncer::UserEventService* user_event_service_;
  std::string app_version_;
  std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(ConsentAuditor);
};

}  // namespace consent_auditor

#endif  // COMPONENTS_CONSENT_AUDITOR_CONSENT_AUDITOR_H_
