// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_member.h"

#include "base/logging.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/notification_type.h"

namespace subtle {

PrefMemberBase::PrefMemberBase()
    : observer_(NULL),
      prefs_(NULL),
      is_synced_(false),
      setting_value_(false) {
}

PrefMemberBase::~PrefMemberBase() {
  if (prefs_ && !pref_name_.empty())
    prefs_->RemovePrefObserver(pref_name_.c_str(), this);
}


void PrefMemberBase::Init(const char* pref_name, PrefService* prefs,
                          NotificationObserver* observer) {
  DCHECK(pref_name);
  DCHECK(prefs);
  DCHECK(pref_name_.empty());  // Check that Init is only called once.
  observer_ = observer;
  prefs_ = prefs;
  pref_name_ = pref_name;
  DCHECK(!pref_name_.empty());

  // Add ourself as a pref observer so we can keep our local value in sync.
  prefs_->AddPrefObserver(pref_name, this);
}

void PrefMemberBase::Destroy() {
  if (prefs_) {
    prefs_->RemovePrefObserver(pref_name_.c_str(), this);
    prefs_ = NULL;
  }
}

bool PrefMemberBase::IsManaged() const {
  DCHECK(!pref_name_.empty());
  const PrefService::Preference* pref =
      prefs_->FindPreference(pref_name_.c_str());
  return pref && pref->IsManaged();
}

void PrefMemberBase::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(!pref_name_.empty());
  DCHECK(NotificationType::PREF_CHANGED == type);
  UpdateValueFromPref();
  is_synced_ = true;
  if (!setting_value_ && observer_)
    observer_->Observe(type, source, details);
}

void PrefMemberBase::VerifyValuePrefName() const {
  DCHECK(!pref_name_.empty());
}

}  // namespace subtle

BooleanPrefMember::BooleanPrefMember() : PrefMember<bool>() {
}

BooleanPrefMember::~BooleanPrefMember() {
}

void BooleanPrefMember::UpdateValueFromPref() const {
  value_ = prefs()->GetBoolean(pref_name().c_str());
}

void BooleanPrefMember::UpdatePref(const bool& value) {
  prefs()->SetBoolean(pref_name().c_str(), value);
}

IntegerPrefMember::IntegerPrefMember() : PrefMember<int>() {
}

IntegerPrefMember::~IntegerPrefMember() {
}

void IntegerPrefMember::UpdateValueFromPref() const {
  value_ = prefs()->GetInteger(pref_name().c_str());
}

void IntegerPrefMember::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name().c_str(), value);
}

RealPrefMember::RealPrefMember() : PrefMember<double>() {
}

RealPrefMember::~RealPrefMember() {
}

void RealPrefMember::UpdateValueFromPref() const {
  value_ = prefs()->GetReal(pref_name().c_str());
}

void RealPrefMember::UpdatePref(const double& value) {
  prefs()->SetReal(pref_name().c_str(), value);
}

StringPrefMember::StringPrefMember() : PrefMember<std::string>() {
}

StringPrefMember::~StringPrefMember() {
}

void StringPrefMember::UpdateValueFromPref() const {
  value_ = prefs()->GetString(pref_name().c_str());
}

void StringPrefMember::UpdatePref(const std::string& value) {
  prefs()->SetString(pref_name().c_str(), value);
}

FilePathPrefMember::FilePathPrefMember() : PrefMember<FilePath>() {
}

FilePathPrefMember::~FilePathPrefMember() {
}

void FilePathPrefMember::UpdateValueFromPref() const {
  value_ = prefs()->GetFilePath(pref_name().c_str());
}

void FilePathPrefMember::UpdatePref(const FilePath& value) {
  prefs()->SetFilePath(pref_name().c_str(), value);
}
