// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"

class MacKeychain;
class NotificationService;

// Implements PasswordStore on top of the OS X Keychain, with an internal
// database for extra metadata. For an overview of the interactions with the
// Keychain, as well as the rationale for some of the behaviors, see the
// Keychain integration design doc:
// http://dev.chromium.org/developers/design-documents/os-x-password-manager-keychain-integration
class PasswordStoreMac : public PasswordStore {
 public:
  // Takes ownership of |keychain| and |login_db|, both of which must be
  // non-NULL.
  PasswordStoreMac(MacKeychain* keychain, LoginDatabase* login_db);

  // Initializes |thread_| and |notification_service_|.
  virtual bool Init();

 protected:
  virtual ~PasswordStoreMac();

  // Schedules tasks on |thread_|.
  virtual void ScheduleTask(Task* task);

 private:
  virtual void ReportMetricsImpl();
  virtual void AddLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void UpdateLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void RemoveLoginImpl(const webkit_glue::PasswordForm& form);
  virtual void RemoveLoginsCreatedBetweenImpl(const base::Time& delete_begin,
                                              const base::Time& delete_end);
  virtual void GetLoginsImpl(GetLoginsRequest* request,
                             const webkit_glue::PasswordForm& form);
  virtual void GetAutofillableLoginsImpl(GetLoginsRequest* request);
  virtual void GetBlacklistLoginsImpl(GetLoginsRequest* request);
  virtual bool FillAutofillableLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);
  virtual bool FillBlacklistLogins(
      std::vector<webkit_glue::PasswordForm*>* forms);

  // Adds the given form to the Keychain if it's something we want to store
  // there (i.e., not a blacklist entry). Returns true if the operation
  // succeeded (either we added successfully, or we didn't need to).
  bool AddToKeychainIfNecessary(const webkit_glue::PasswordForm& form);

  // Returns true if our database contains a form that exactly matches the given
  // keychain form.
  bool DatabaseHasFormMatchingKeychainForm(
      const webkit_glue::PasswordForm& form);

  // Returns all the Keychain entries that we own but no longer have
  // corresponding metadata for in our database.
  // Caller is responsible for deleting the forms.
  std::vector<webkit_glue::PasswordForm*> GetUnusedKeychainForms();

  // Removes the given forms from the database.
  void RemoveDatabaseForms(
      const std::vector<webkit_glue::PasswordForm*>& forms);

  // Removes the given forms from the Keychain.
  void RemoveKeychainForms(
      const std::vector<webkit_glue::PasswordForm*>& forms);

  // Allows the creation of |notification_service_| to be scheduled on the right
  // thread.
  void CreateNotificationService();

  scoped_ptr<MacKeychain> keychain_;
  scoped_ptr<LoginDatabase> login_metadata_db_;

  // Thread that the synchronous methods are run on.
  scoped_ptr<base::Thread> thread_;

  // Since we aren't running on a well-known thread but still want to send out
  // notifications, we need to run our own service.
  scoped_ptr<NotificationService> notification_service_;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreMac);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_MAC_H_
