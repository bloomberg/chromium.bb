// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_

#include <string>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace ntp_snippets {

// Stores and vend fresh content data for the NTP.
class NTPSnippetsService : public KeyedService {
 public:
  // |application_language_code| should be a ISO 639-1 compliant string. Aka
  // 'en' or 'en-US'. Note that this code should only specify the language, not
  // the locale, so 'en_US' (english language with US locale) and 'en-GB_US'
  // (British english person in the US) are not language code.
  explicit NTPSnippetsService(const std::string& application_language_code);
  ~NTPSnippetsService() override;

  void Shutdown() override;

  // TODO(crbug.com/547046): Add API.
 private:
  // The ISO 639-1 code of the language used by the application.
  const std::string application_language_code_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsService);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_SERVICE_H_
