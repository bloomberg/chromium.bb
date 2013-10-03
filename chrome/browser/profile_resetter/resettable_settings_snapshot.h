// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_RESETTABLE_SETTINGS_SNAPSHOT_H_
#define CHROME_BROWSER_PROFILE_RESETTER_RESETTABLE_SETTINGS_SNAPSHOT_H_

#include "base/basictypes.h"
#include "chrome/browser/prefs/session_startup_pref.h"

namespace base {
class ListValue;
}

// ResettableSettingsSnapshot captures some settings values at constructor. It
// can calculate the difference between two snapshots. That is, modified fields.
class ResettableSettingsSnapshot {
 public:
  // ExtensionList is a vector of pairs. The first component is the extension
  // id, the second is the name.
  typedef std::vector<std::pair<std::string, std::string> > ExtensionList;
  // All types of settings handled by this class.
  enum Field {
    STARTUP_MODE = 1 << 0,
    HOMEPAGE = 1 << 1,
    DSE_URL = 1 << 2,
    EXTENSIONS = 1 << 3,

    ALL_FIELDS = STARTUP_MODE | HOMEPAGE | DSE_URL | EXTENSIONS,
  };

  explicit ResettableSettingsSnapshot(Profile* profile);
  ~ResettableSettingsSnapshot();

  // Getters.
  const std::vector<GURL>& startup_urls() const { return startup_.urls; }

  SessionStartupPref::Type startup_type() const { return startup_.type; }

  const std::string& homepage() const { return homepage_; }

  bool homepage_is_ntp() const { return homepage_is_ntp_; }

  const std::string& dse_url() const { return dse_url_; }

  const ExtensionList& enabled_extensions() const {
    return enabled_extensions_;
  }

  // Substitutes |enabled_extensions_| with
  // |enabled_extensions_|\|snapshot.enabled_extensions_|.
  void Subtract(const ResettableSettingsSnapshot& snapshot);

  // For each member 'm' compares |this->m| with |snapshot.m| and sets the
  // corresponding |ResetableSettingsSnapshot::Field| bit to 1 in case of
  // difference.
  // The return value is a bit mask of Field values signifying which members
  // were different.
  int FindDifferentFields(const ResettableSettingsSnapshot& snapshot) const;

 private:
  // Startup pages. URLs are always stored sorted.
  SessionStartupPref startup_;

  std::string homepage_;
  bool homepage_is_ntp_;

  // Default search engine.
  std::string dse_url_;

  // List of pairs [id, name] for enabled extensions. Always sorted.
  ExtensionList enabled_extensions_;

  DISALLOW_COPY_AND_ASSIGN(ResettableSettingsSnapshot);
};

// The user of ResettableSettingsSnapshot. Don't modify the numeric values below
// because they are serialized.
enum SnapshotCaller {
  PROFILE_RESET_WEBUI = 0,
};

// Serializes specified |snapshot| members to JSON format. |field_mask| is a bit
// mask of ResettableSettingsSnapshot::Field values.
std::string SerializeSettingsReport(const ResettableSettingsSnapshot& snapshot,
                                    int field_mask,
                                    SnapshotCaller source);

// Sends |report| as a feedback. |report| is supposed to be result of
// SerializeSettingsReport().
void SendSettingsFeedback(const std::string& report, Profile* profile);

// Returns list of key/value pairs for all reported information from the
// |profile| and some additional fields.
base::ListValue* GetReadableFeedback(Profile* profile);

#endif  // CHROME_BROWSER_PROFILE_RESETTER_RESETTABLE_SETTINGS_SNAPSHOT_H_
