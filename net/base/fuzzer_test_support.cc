// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"

namespace {

// This translation unit provides a static initializer to set up ICU.
//
// ICU is used internally by GURL, which is used throughout the //net code.
// Initializing ICU is important to prevent fuzztests from asserting when
// handling non-ASCII urls.
//
// Note that in general static initializers are not allowed, however this is
// just being used by test code.
struct InitICU {
  InitICU() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit_manager;
};

InitICU* init_icu = new InitICU();

}  // namespace
