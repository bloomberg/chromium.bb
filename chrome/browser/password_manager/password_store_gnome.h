// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME_H_

extern "C" {
#include <gnome-keyring.h>
}

#include <vector>

#include "base/lock.h"
#include "chrome/browser/password_manager/login_database.h"
#include "chrome/browser/password_manager/password_store.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/webdata/web_data_service.h"

class Profile;
class Task;

// PasswordStore implementation using GNOME Keyring.
class PasswordStoreGnome : public PasswordStore {
 public:
  PasswordStoreGnome(LoginDatabase* login_db,
                     Profile* profile,
                     WebDataService* web_data_service);

  virtual bool Init();

 private:
  virtual ~PasswordStoreGnome();

  // Implements PasswordStore interface.
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

  // Helper for AddLoginImpl() and UpdateLoginImpl() with a success status.
  bool AddLoginHelper(const webkit_glue::PasswordForm& form,
                      const base::Time& date_created);

  // Helper for FillAutofillableLogins() and FillBlacklistLogins().
  bool FillSomeLogins(bool autofillable,
                      std::vector<webkit_glue::PasswordForm*>* forms);

  // Parse all the results from the given GList into a
  // vector<PasswordForm*>, and free the GList. PasswordForms are
  // allocated on the heap, and should be deleted by the consumer.
  void FillFormVector(GList* found,
                      std::vector<webkit_glue::PasswordForm*>* forms);

  static const GnomeKeyringPasswordSchema kGnomeSchema;

  DISALLOW_COPY_AND_ASSIGN(PasswordStoreGnome);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_GNOME_H_
