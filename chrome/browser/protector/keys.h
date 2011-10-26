// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_KEYS_H_
#define CHROME_BROWSER_PROTECTOR_KEYS_H_
#pragma once

namespace protector {

// When changing the key, be sure to add migration code to keep user's
// settings safe.
extern const char kProtectorSigningKey[];

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_KEYS_H_

