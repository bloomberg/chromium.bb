// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_
#define CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_

#include <string>

// These are instructions to generate binary code for JTL programs.

#define VALUE_FALSE std::string(1, '\x00')
#define VALUE_TRUE std::string(1, '\x01')

#define OP_NAVIGATE(key) std::string(1, '\x00') + key
#define OP_NAVIGATE_ANY std::string(1, '\x01')
#define OP_NAVIGATE_BACK std::string(1, '\x02')
#define OP_STORE_BOOL(name, value) std::string(1, '\x10') + name + value
#define OP_COMPARE_STORED_BOOL(name, value, default_value) \
    std::string(1, '\x11') + name + value + default_value
#define OP_STORE_HASH(name, value) std::string(1, '\x12') + name + value
#define OP_COMPARE_STORED_HASH(name, value, default_value) \
    std::string(1, '\x13') + name + value + default_value
#define OP_STORE_NODE_BOOL(name) std::string(1, '\x14') + name
#define OP_STORE_NODE_HASH(name) std::string(1, '\x15') + name
#define OP_STORE_NODE_REGISTERABLE_DOMAIN_HASH(name) \
    std::string(1, '\x16') + name
#define OP_COMPARE_NODE_BOOL(value) std::string(1, '\x20') + value
#define OP_COMPARE_NODE_HASH(value) std::string(1, '\x21') + value
#define OP_COMPARE_NODE_HASH_NOT(value) std::string(1, '\x22') + value
#define OP_COMPARE_NODE_TO_STORED_BOOL(name) std::string(1, '\x23') + name
#define OP_COMPARE_NODE_TO_STORED_HASH(name) std::string(1, '\x24') + name
#define OP_COMPARE_NODE_SUBSTRING(pattern, pattern_length, pattern_sum) \
    std::string(1, '\x25') + pattern + pattern_length + pattern_sum
#define OP_STOP_EXECUTING_SENTENCE std::string(1, '\x30')
#define OP_END_OF_SENTENCE std::string(1, '\x31')

#endif  // CHROME_BROWSER_PROFILE_RESETTER_JTL_INSTRUCTIONS_H_
