// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_VALIDATION_DB_H_
#define CHROME_NACL_NACL_VALIDATION_DB_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class NaClValidationDB {
 public:
  NaClValidationDB() {}
  virtual ~NaClValidationDB() {}

  virtual bool QueryKnownToValidate(const std::string& signature) = 0;
  virtual void SetKnownToValidate(const std::string& signature) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NaClValidationDB);
};

#endif  // CHROME_NACL_NACL_VALIDATION_DB_H_
