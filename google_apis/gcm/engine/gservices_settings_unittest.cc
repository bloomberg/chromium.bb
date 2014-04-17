// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "google_apis/gcm/engine/gservices_settings.h"
#include "google_apis/gcm/engine/registration_info.h"
#include "google_apis/gcm/gcm_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

const int64 kAlternativeCheckinInterval = 2000LL;
const char kAlternativeCheckinURL[] = "http://alternative.url/checkin";
const char kAlternativeMCSHostname[] = "http://alternative.gcm.host";
const int kAlternativeMCSSecurePort = 443;
const char kAlternativeRegistrationURL[] =
    "http://alternative.url/registration";

const int64 kDefaultCheckinInterval = 2 * 24 * 60 * 60;  // seconds = 2 days.
const char kDefaultCheckinURL[] = "https://android.clients.google.com/checkin";
const char kDefaultMCSHostname[] = "https://mtalk.google.com";
const int kDefaultMCSSecurePort = 5228;
const char kDefaultRegistrationURL[] =
    "https://android.clients.google.com/c2dm/register3";

class FakeGCMStore : public GCMStore {
 public:
  FakeGCMStore();
  virtual ~FakeGCMStore();

  virtual void Load(const gcm::GCMStore::LoadCallback& callback) OVERRIDE {}
  virtual void Close() OVERRIDE {}
  virtual void Destroy(const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {
  }
  virtual void SetDeviceCredentials(
      uint64 device_android_id,
      uint64 device_security_token,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void AddRegistration(
      const std::string& app_id,
      const linked_ptr<gcm::RegistrationInfo>& registration,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void RemoveRegistration(
      const std::string& app_id,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void AddIncomingMessage(
      const std::string& persistent_id,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void RemoveIncomingMessage(
      const std::string& persistent_id,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void RemoveIncomingMessages(
      const PersistentIdList& persistent_ids,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual bool AddOutgoingMessage(
      const std::string& persistent_id,
      const gcm::MCSMessage& message,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {
    return true;
  }
  virtual void OverwriteOutgoingMessage(
      const std::string& persistent_id,
      const gcm::MCSMessage& message,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void RemoveOutgoingMessage(
      const std::string& persistent_id,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void RemoveOutgoingMessages(
      const PersistentIdList& persistent_ids,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}
  virtual void SetLastCheckinTime(
      const base::Time& last_checkin_time,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {}

  // G-service settings handling.
  virtual void SetGServicesSettings(
      const std::map<std::string, std::string>& settings,
      const std::string& settings_digest,
      const gcm::GCMStore::UpdateCallback& callback) OVERRIDE {
    settings_saved_ = true;
  }

  void Reset() {
    settings_saved_ = false;
  }

  bool settings_saved() const { return settings_saved_; }

 private:
  bool settings_saved_;
};

FakeGCMStore::FakeGCMStore() : settings_saved_(false) {}

FakeGCMStore::~FakeGCMStore() {}

}  // namespace

class GServicesSettingsTest : public testing::Test {
 public:
  GServicesSettingsTest();
  virtual ~GServicesSettingsTest();

  virtual void SetUp() OVERRIDE;

  void CheckAllSetToDefault();
  void CheckAllSetToAlternative();
  void SetWithAlternativeSettings(
      checkin_proto::AndroidCheckinResponse& checkin_response);

  GServicesSettings& settings() {
    return gserivces_settings_;
  }

  const std::map<std::string, std::string>& alternative_settings() {
    return alternative_settings_;
  }

  FakeGCMStore& gcm_store() { return gcm_store_; }

 private:
  FakeGCMStore gcm_store_;
  GServicesSettings gserivces_settings_;
  std::map<std::string, std::string> alternative_settings_;
};

GServicesSettingsTest::GServicesSettingsTest()
    : gserivces_settings_(&gcm_store_) {
}

GServicesSettingsTest::~GServicesSettingsTest() {}

void GServicesSettingsTest::SetUp() {
  alternative_settings_["checkin_interval"] =
      base::Int64ToString(kAlternativeCheckinInterval);
  alternative_settings_["checkin_url"] = kAlternativeCheckinURL;
  alternative_settings_["gcm_hostname"] = kAlternativeMCSHostname;
  alternative_settings_["gcm_secure_port"] =
      base::IntToString(kAlternativeMCSSecurePort);
  alternative_settings_["gcm_registration_url"] = kAlternativeRegistrationURL;
}

void GServicesSettingsTest::CheckAllSetToDefault() {
  EXPECT_EQ(kDefaultCheckinInterval, settings().checkin_interval());
  EXPECT_EQ(kDefaultCheckinURL, settings().checkin_url());
  EXPECT_EQ(kDefaultMCSHostname, settings().mcs_hostname());
  EXPECT_EQ(kDefaultMCSSecurePort, settings().mcs_secure_port());
  EXPECT_EQ(kDefaultRegistrationURL, settings().registration_url());
}

void GServicesSettingsTest::CheckAllSetToAlternative() {
  EXPECT_EQ(kAlternativeCheckinInterval, settings().checkin_interval());
  EXPECT_EQ(kAlternativeCheckinURL, settings().checkin_url());
  EXPECT_EQ(kAlternativeMCSHostname, settings().mcs_hostname());
  EXPECT_EQ(kAlternativeMCSSecurePort, settings().mcs_secure_port());
  EXPECT_EQ(kAlternativeRegistrationURL, settings().registration_url());
}

void GServicesSettingsTest::SetWithAlternativeSettings(
    checkin_proto::AndroidCheckinResponse& checkin_response) {
  for (std::map<std::string, std::string>::const_iterator iter =
           alternative_settings_.begin();
       iter != alternative_settings_.end(); ++iter) {
    checkin_proto::GservicesSetting* setting = checkin_response.add_setting();
    setting->set_name(iter->first);
    setting->set_value(iter->second);
  }
}

// Verifies default values of the G-services settings and settings digest.
TEST_F(GServicesSettingsTest, DefaultSettingsAndDigest) {
  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that the settings are set correctly based on the load result.
TEST_F(GServicesSettingsTest, UpdateFromLoadResult) {
  GCMStore::LoadResult result;
  result.gservices_settings = alternative_settings();
  result.gservices_digest = "digest_value";
  settings().UpdateFromLoadResult(result);

  CheckAllSetToAlternative();
  EXPECT_EQ("digest_value", settings().digest());
}

// Verifies that the settings are set correctly after parsing a checkin
// response.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponse) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  checkin_response.set_digest("digest_value");
  SetWithAlternativeSettings(checkin_response);

  settings().UpdateFromCheckinResponse(checkin_response);
  EXPECT_TRUE(gcm_store().settings_saved());

  CheckAllSetToAlternative();
  EXPECT_EQ("digest_value", settings().digest());
}

// Verifies that no update is done, when a checkin response misses digest.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseNoDigest) {
  checkin_proto::AndroidCheckinResponse checkin_response;

  SetWithAlternativeSettings(checkin_response);
  settings().UpdateFromCheckinResponse(checkin_response);
  EXPECT_FALSE(gcm_store().settings_saved());

  CheckAllSetToDefault();
  EXPECT_EQ(std::string(), settings().digest());
}

// Verifies that no update is done, when a checkin response digest is the same.
TEST_F(GServicesSettingsTest, UpdateFromCheckinResponseSameDigest) {
  GCMStore::LoadResult load_result;
  load_result.gservices_digest = "old_digest";
  load_result.gservices_settings = alternative_settings();
  settings().UpdateFromLoadResult(load_result);

  checkin_proto::AndroidCheckinResponse checkin_response;
  checkin_response.set_digest("old_digest");
  SetWithAlternativeSettings(checkin_response);
  settings().UpdateFromCheckinResponse(checkin_response);
  EXPECT_FALSE(gcm_store().settings_saved());

  CheckAllSetToAlternative();
  EXPECT_EQ("old_digest", settings().digest());
}

}  // namespace gcm
