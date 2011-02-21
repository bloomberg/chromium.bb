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
#include "base/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/notification_observer.h"

class PrefService;

namespace subtle {

class PrefMemberBase : public NotificationObserver {
 protected:
  class Internal : public base::RefCountedThreadSafe<Internal> {
   public:
    Internal();

    // Update the value, either by calling |UpdateValueInternal| directly
    // or by dispatching to the right thread.
    // Takes ownership of |value|.
    virtual void UpdateValue(Value* value) const;

    void MoveToThread(BrowserThread::ID thread_id);

   protected:
    friend class base::RefCountedThreadSafe<Internal>;
    virtual ~Internal();

    void CheckOnCorrectThread() const {
      DCHECK(IsOnCorrectThread());
    }

   private:
    // This method actually updates the value. It should only be called from
    // the thread the PrefMember is on.
    virtual bool UpdateValueInternal(const Value& value) const = 0;

    bool IsOnCorrectThread() const;

    BrowserThread::ID thread_id_;

    DISALLOW_COPY_AND_ASSIGN(Internal);
  };

  PrefMemberBase();
  virtual ~PrefMemberBase();

  // See PrefMember<> for description.
  void Init(const char* pref_name, PrefService* prefs,
            NotificationObserver* observer);

  virtual void CreateInternal() const = 0;

  // See PrefMember<> for description.
  void Destroy();

  // See PrefMember<> for description.
  bool IsManaged() const;

  void MoveToThread(BrowserThread::ID thread_id);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void VerifyValuePrefName() const;

  // This method is used to do the actual sync with the preference.
  // Note: it is logically const, because it doesn't modify the state
  // seen by the outside world. It is just doing a lazy load behind the scenes.
  virtual void UpdateValueFromPref() const;

  const std::string& pref_name() const { return pref_name_; }
  PrefService* prefs() { return prefs_; }
  const PrefService* prefs() const { return prefs_; }

  virtual Internal* internal() const = 0;

 // Ordered the members to compact the class instance.
 private:
  std::string pref_name_;
  NotificationObserver* observer_;
  PrefService* prefs_;

 protected:
  bool setting_value_;
};

}  // namespace subtle

template <typename ValueType>
class PrefMember : public subtle::PrefMemberBase {
 public:
  // Defer initialization to an Init method so it's easy to make this class be
  // a member variable.
  PrefMember() {}
  virtual ~PrefMember() {}

  // Do the actual initialization of the class.  |observer| may be null if you
  // don't want any notifications of changes.
  // This method should only be called on the UI thread.
  void Init(const char* pref_name, PrefService* prefs,
            NotificationObserver* observer) {
    subtle::PrefMemberBase::Init(pref_name, prefs, observer);
  }

  // Unsubscribes the PrefMember from the PrefService. After calling this
  // function, the PrefMember may not be used any more.
  // This method should only be called on the UI thread.
  void Destroy() {
    subtle::PrefMemberBase::Destroy();
  }

  // Moves the PrefMember to another thread, allowing read accesses from there.
  // Changes from the PrefService will be propagated asynchronously
  // via PostTask.
  // This method should only be used from the thread the PrefMember is currently
  // on, which is the UI thread by default.
  void MoveToThread(BrowserThread::ID thread_id) {
    subtle::PrefMemberBase::MoveToThread(thread_id);
  }

  // Check whether the pref is managed, i.e. controlled externally through
  // enterprise configuration management (e.g. windows group policy). Returns
  // false for unknown prefs.
  // This method should only be called on the UI thread.
  bool IsManaged() const {
    return subtle::PrefMemberBase::IsManaged();
  }

  // Retrieve the value of the member variable.
  // This method should only be used from the thread the PrefMember is currently
  // on, which is the UI thread unless changed by |MoveToThread|.
  ValueType GetValue() const {
    VerifyValuePrefName();
    // We lazily fetch the value from the pref service the first time GetValue
    // is called.
    if (!internal_.get())
      UpdateValueFromPref();
    return internal_->value();
  }

  // Provided as a convenience.
  ValueType operator*() const {
    return GetValue();
  }

  // Set the value of the member variable.
  // This method should only be called on the UI thread.
  void SetValue(const ValueType& value) {
    VerifyValuePrefName();
    setting_value_ = true;
    UpdatePref(value);
    setting_value_ = false;
  }

  // Set the value of the member variable if it is not managed.
  // This method should only be called on the UI thread.
  void SetValueIfNotManaged(const ValueType& value) {
    if (!IsManaged()) {
      SetValue(value);
    }
  }

 private:
  class Internal : public subtle::PrefMemberBase::Internal {
   public:
    Internal() : value_(ValueType()) {}

    ValueType value() {
      CheckOnCorrectThread();
      return value_;
    }

   protected:
    virtual ~Internal() {}

    virtual bool UpdateValueInternal(const Value& value) const;

    // We cache the value of the pref so we don't have to keep walking the pref
    // tree.
    mutable ValueType value_;

    DISALLOW_COPY_AND_ASSIGN(Internal);
  };

  virtual Internal* internal() const { return internal_; }
  virtual void CreateInternal() const {
    internal_ = new Internal();
  }

  // This method is used to do the actual sync with pref of the specified type.
  virtual void UpdatePref(const ValueType& value);

  mutable scoped_refptr<Internal> internal_;

  DISALLOW_COPY_AND_ASSIGN(Internal);
};

typedef PrefMember<bool> BooleanPrefMember;
typedef PrefMember<int> IntegerPrefMember;
typedef PrefMember<double> DoublePrefMember;
typedef PrefMember<std::string> StringPrefMember;
typedef PrefMember<FilePath> FilePathPrefMember;

#endif  // CHROME_BROWSER_PREFS_PREF_MEMBER_H_
