// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CHANGE_H__
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CHANGE_H__
#pragma once

#include <vector>

#include "webkit/glue/password_form.h"

class PasswordStoreChange {
 public:
  enum Type {
    ADD,
    UPDATE,
    REMOVE,
  };

  PasswordStoreChange(Type type, const webkit_glue::PasswordForm& form)
      : type_(type), form_(form) {
  }
  virtual ~PasswordStoreChange() {}

  Type type() const { return type_; }
  const webkit_glue::PasswordForm& form() const { return form_; }

  bool operator==(const PasswordStoreChange& other) const {
    return type() == other.type() &&
           form().signon_realm == other.form().signon_realm &&
           form().origin == other.form().origin &&
           form().action == other.form().action &&
           form().submit_element == other.form().submit_element &&
           form().username_element == other.form().username_element &&
           form().username_value == other.form().username_value &&
           form().password_element == other.form().password_element &&
           form().password_value == other.form().password_value &&
           form().old_password_element == other.form().old_password_element &&
           form().old_password_value == other.form().old_password_value &&
           form().ssl_valid == other.form().ssl_valid &&
           form().preferred == other.form().preferred &&
           form().date_created == other.form().date_created &&
           form().blacklisted_by_user == other.form().blacklisted_by_user;
  }

 private:
  Type type_;
  webkit_glue::PasswordForm form_;
};

typedef std::vector<PasswordStoreChange> PasswordStoreChangeList;

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_CHANGE_H_
