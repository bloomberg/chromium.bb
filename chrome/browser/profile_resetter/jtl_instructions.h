// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_
#define CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_

#include <string>

// These are instructions to generate binary code for JTL programs.

#define VALUE_FALSE std::string("\x00", 1)
#define VALUE_TRUE std::string("\x01", 1)

#define OP_NAVIGATE(key) std::string("\x00", 1) + key
#define OP_NAVIGATE_ANY std::string("\x01", 1)
#define OP_NAVIGATE_BACK std::string("\x02", 1)
#define OP_STORE_BOOL(name, value) std::string("\x10") + name + value
#define OP_COMPARE_STORED_BOOL(name, value, default_value) \
    std::string("\x11", 1) + name + value + default_value
#define OP_STORE_HASH(name, value) std::string("\x12") + name + value
#define OP_COMPARE_STORED_HASH(name, value, default_value) \
    std::string("\x13", 1) + name + value + default_value
#define OP_COMPARE_NODE_BOOL(value) std::string("\x20", 1) + value
#define OP_COMPARE_NODE_HASH(value) std::string("\x21", 1) + value
#define OP_STOP_EXECUTING_SENTENCE std::string("\x30", 1)
#define OP_END_OF_SENTENCE std::string("\x31", 1)

#endif  // CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_
