// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that stays in sync with a preference (bool, int, real,
// string or filepath).  For example:
//
// class MyClass {
//  public:
//   MyClass(PrefService* prefs) {
//     my_string_.Init(prefs::kHomePage, prefs, NULL /* no observer */);
//   }
//  private:
//   StringPrefMember my_string_;
// };
//
// my_string_ should stay in sync with the prefs::kHomePage pref and will
// update if either the pref changes or if my_string_.SetValue is called.
//
// An optional observer can be passed into the Init method which can be used to
// notify MyClass of changes. Note that if you use SetValue(), the observer
// will not be notified.

#ifndef CHROME_BROWSER_PREFS_PREF_MEMBER_H_
#define CHROME_BROWSER_PREFS_PREF_MEMBER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "chrome/common/notification_observer.h"

class PrefService;

namespace subtle {

class PrefMemberBase : public NotificationObserver {
 protected:
  PrefMemberBase();
  virtual ~PrefMemberBase();

  // See PrefMember<> for description.
  void Init(const char* pref_name, PrefService* prefs,
            NotificationObserver* observer);

  // See PrefMember<> for description.
  void Destroy();

  // See PrefMember<> for description.
  bool IsManaged() const;

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void VerifyValuePrefName() const;

  // This methods is used to do the actual sync with pref of the specified type.
  // Note: this method is logically const, because it doesn't modify the state
  // seen by the outside world. It is just doing a lazy load behind the scenes.
  virtual void UpdateValueFromPref() const = 0;

  const std::string& pref_name() const { return pref_name_; }
  PrefService* prefs() { return prefs_; }
  const PrefService* prefs() const { return prefs_; }

 // Ordered the members to compact the class instance.
 private:
  std::string pref_name_;
  NotificationObserver* observer_;
  PrefService* prefs_;

 protected:
  mutable bool is_synced_;
  bool setting_value_;
};

}  // namespace subtle


template <typename ValueType>
class PrefMember : public subtle::PrefMemberBase {
 public:
  // Defer initialization to an Init method so it's easy to make this class be
  // a member variable.
  PrefMember() : value_(ValueType()) {}
  virtual ~PrefMember() {}

  // Do the actual initialization of the class.  |observer| may be null if you
  // don't want any notifications of changes.
  void Init(const char* pref_name, PrefService* prefs,
            NotificationObserver* observer) {
    subtle::PrefMemberBase::Init(pref_name, prefs, observer);
  }

  // Unsubscribes the PrefMember from the PrefService. After calling this
  // function, the PrefMember may not be used any more.
  void Destroy() {
    subtle::PrefMemberBase::Destroy();
  }

  // Check whether the pref is managed, i.e. controlled externally through
  // enterprise configuration management (e.g. windows group policy). Returns
  // false for unknown prefs.
  bool IsManaged() const {
    return subtle::PrefMemberBase::IsManaged();
  }

  // Retrieve the value of the member variable.
  ValueType GetValue() const {
    VerifyValuePrefName();
    // We lazily fetch the value from the pref service the first time GetValue
    // is called.
    if (!is_synced_) {
      UpdateValueFromPref();
      is_synced_ = true;
    }
    return value_;
  }

  // Provided as a convenience.
  ValueType operator*() const {
    return GetValue();
  }

  // Set the value of the member variable.
  void SetValue(const ValueType& value) {
    VerifyValuePrefName();
    setting_value_ = true;
    UpdatePref(value);
    setting_value_ = false;
  }

  // Set the value of the member variable if it is not managed.
  void SetValueIfNotManaged(const ValueType& value) {
    if (!IsManaged()) {
      SetValue(value);
    }
  }

 protected:
  // This methods is used to do the actual sync with pref of the specified type.
  virtual void UpdatePref(const ValueType& value) = 0;

  // We cache the value of the pref so we don't have to keep walking the pref
  // tree.
  mutable ValueType value_;
};

///////////////////////////////////////////////////////////////////////////////
// Implementations of Boolean, Integer, Real, and String PrefMember below.

class BooleanPrefMember : public PrefMember<bool> {
 public:
  BooleanPrefMember();
  virtual ~BooleanPrefMember();

 protected:
  virtual void UpdateValueFromPref() const;
  virtual void UpdatePref(const bool& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(BooleanPrefMember);
};

class IntegerPrefMember : public PrefMember<int> {
 public:
  IntegerPrefMember();
  virtual ~IntegerPrefMember();

 protected:
  virtual void UpdateValueFromPref() const;
  virtual void UpdatePref(const int& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(IntegerPrefMember);
};

class DoublePrefMember : public PrefMember<double> {
 public:
  DoublePrefMember();
  virtual ~DoublePrefMember();

 protected:
  virtual void UpdateValueFromPref() const;
  virtual void UpdatePref(const double& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(DoublePrefMember);
};

class StringPrefMember : public PrefMember<std::string> {
 public:
  StringPrefMember();
  virtual ~StringPrefMember();

 protected:
  virtual void UpdateValueFromPref() const;
  virtual void UpdatePref(const std::string& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(StringPrefMember);
};

class FilePathPrefMember : public PrefMember<FilePath> {
 public:
  FilePathPrefMember();
  virtual ~FilePathPrefMember();

 protected:
  virtual void UpdateValueFromPref() const;
  virtual void UpdatePref(const FilePath& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(FilePathPrefMember);
};

#endif  // CHROME_BROWSER_PREFS_PREF_MEMBER_H_
